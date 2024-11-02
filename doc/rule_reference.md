# JARY Rule Reference
The Jary language shares a similar syntax with another language called Yara. But thats the end of its similarity, because the use case of each language is completely different. It may help if you know yara, but you still need to read everything on this page.

The grammar is very simple, you only need to know these concepts: 
- `import` 
- `ingress` 
- `rule` 
- `section` 
- `identifier`
- `expressions`
- `comments`

Example:
```
// import statement
import mymodule 

// ingress declaration
ingress user {
    // field section
    field:
        name string
}

// rule declaration
rule my_rule {
    // match section
    match:
        // match expression
        $user.name exact "root"
        $user within 10s
}
```

## Import statement
The `import` keyword is used to add additional functionality to the Jary rule file by loading an `.so` object within the file system. Under the hood, Jary use `dlopen` to load the dynamic library using the system `ld` path to perform the resolution. 

For example, if I'm importing `mark` module, 
```
import mark
```
it will try to locate the file `libmark.so` in my system resolution path:
```
/lib64
/usr/lib64
/usr/local/lib64
/lib
/usr/lib
/usr/local/lib
...
```
> These are just my system library paths, yours can be different

The `.so` object must define two mandatory function `module_load` and `module_unload` to be loadable by Jary. 

## Ingress declaration
The `ingress` keyword is used to declare the structure of an expected event denoted by an `identifier`. If a user event is expected, which contain two data field called `name` and `age`, then it must have a corresponding `ingress` declaration like this:

```
ingress user {
    field:
        name string
        age long
}
```



The ingress declaration only have the `field` section used to declare a set of fields.

A field declaration only compose of an `identifier` and a type:

```
<identifier> <type>
```

The `identifier` of a field cannot start with two underlines: `__` since any field with that suffix is reserved by `Jary`.

> More ingress section will be added in the future

The data field can be of any of these types:
- `long` 64-bit signed number
- `string` null terminated character array
- `bool` truthy value (0 false, non 0 true)
- `ulong` 64-bit signed number (kinda useless)

> More types will be implemented in the future

Jary will only accept event that has an `ingress` declaration, so it won't accept arbitrary event. This is to prevent confusion when reading the rules.

## Rule declaration
The `rule` keyword is used to declare a group of predicate/logical expression that dictates whether a certain action must be taken. Every rule must also be denoted by an `identifier`.

>This is the main feature of Jary, and the only thing that actually gets compiled into bytecode for execution. The rest are just contextual information and only used during compilation phase.

A rule declaration accepts only the following sections:
1. `match` required
2. `condition` optional
3. `output` semi-optional
4. `action` semi-optional

The flow of the rule starts from `match` and ends with `action`. But the written order of the section does not matter since it will be restructured internally. 

### `match:`
A rule match section define a set of logic used to select events stored within an in-memory `sqlite3` database. It expects a unique value type called `match value`. 

A match value is only returned by these operators:
| Operator | Description | Usage   |
|----------|-------------|---------|
|`between` | range check between integer | `$user.age between 10..19`|
|`exact`   | compare two strings | `$user.name exact "root"`|
| `equal`  | compare two integer | `$user.age equal 3` |
| `join`   | cross join two events by field | `$user.name join $duty.person` |
| `regex`  | match strings againts a regex pattern | `$user.name regex /john/` |
| `within` | filter event by arrival time relative to current local time | `$user within 10s` |

> `regex` operator is still not stable yet, **DO NOT USE**

Each of these operators only work with either an `event` or a `field` as its left hand side operand.

An event is denoted by a `$` operator followed immediately by an `identifier` corresponding to an `ingress` declaration. 

```
// user event
$user
```

A `field` is the data carried by an `event` and is accessed using the `.` operator.
```
// name field
$user.name 
```

So an example of `match` section would typically look like these:

```
match:
    $user.name exact "root"
    $user within 10s
```

### `condition:`
A rule condition section define a set of predicates used to determine if an event should cascade down to the `output` and `action` section. It only expects predicates, and can have up to `255` individual predicates. 

A predicate here just means an expression that returns a `boolean` value and delimited by a ****newline**** like:

```
// predicate 1
5 > 3

// predicate 2
2 == 2 or "hello" == "hello"

// predicate 3
5 < 3 and 1 == 1 
```

The predicates are checked from top to bottom, and work in a **short-circuit** fashion. Basically just like an `and` operator:

```
condition:
    // this fails
    1 == 2

    // so this wont be reached
    $user.age < 10 and 1 == 1
```

#### `match` vs `condition`
Some of you might find the `condition` section to be very counter intuitive as it sound quite similar to the `match` section. The key difference between them lies on where they apply their operation. 

To understand it, we have to go into abit of `SQL`. The `match` section is simply just performing an `SQL` query where all of its `match values` is placed in the `WHERE` clause within a `SELECT` statement. 

So basically this snippet:
```
match:
    $user.name exact "root" 
```
is the equivalent to this `SQL` code.
```sql
SELECT user AS 'user.name' WHERE user.name = 'root'
```

Meanwhile, the `condition` section works on the resulted **rows** from the previous `SQL` query. And because the rows can be more than one, the `condition` is essentially performing a loop on each row to determine which row should be passed into the `output` and `action` section. 

So it would somewhat look like this in code:

```js
// Just a pseudo code

for (data in rows) {
    if (check_condition(data)) {
        run_outputs(data)
        run_actions(data)
    }
}
```

> In an attempt to differentiate them, Jary use two different set of operators. One is specifically in `match` section, and the other one to be used in any place that expect an expression.

### `output:`
A rule output section is used to pass an array of values to every callback function listening to the rule. It expects a list of expression which will be evaluated before the `action` section. 

The output values are then passed in the exact order of the listed expression, where it starts from the 0th index, into the callback:

```
output:
    1 + 1             # 0th value
    $user.name        # 1th value
    ...               
    $user.age         # nth value
```

### `action:`
A rule action section is used to call a set of special functions. These functions are defined by module developers and can only be used within this section. 

Using an action function would look something like this:

```
// this is just a made up library
import log

rule {
...
    action:
        // info() is an action function
        log.info("hello")
        // warn() is an action function
        log.warn("world")
}
```

the call order is evaluated from top to bottom, so `log.info` first then `log.warn` last.

> Standard modules will be added in the future. **Feedback needed**

## Section declaration
A section describe a special context of a declaration. Any keyword that ended with `:` within a declaration is considered a section, and there are only a handful of them for either `ingress` and `rule` declaration. 

Most section contains a multiple of **statement expression**, that expects a specific return type value. 

> The only exception to this being the `field` section as it is only contextual information, and does not expect a value

Here's an example of section:

```
psuedo_section:
  value1
  value2
  value3
```

The value within a section is read from top to bottom, and the order of the value plays an important role in different sections.

There are currently only the listed sections available:
- `field`
- `match`
- `condition`
- `output`
- `action`

> More sections will be added if circumstance requires it

## Identifiers
An identifier is used to identify **one** declaration and one only. Currently jary does not have the concept of `scopes` since it does not have local variables. 

> I will add a way to store temporary values in the future

Any identifier is consider invalid if it violates the following:
- cannot match againts this regex pattern: `[_a-zA-Z]+[_a-zA-Z0-9]*`
- is an exact match to a reserve word

> Using two underlines `__` at the start of an identifier is also considered invalid, but weakly enforced.

## Reserved words

```
import, ingress, rule,
if, else, elif, fi, for, range, then,
join, within, between, exact, equal, regex
field, match, input, action, output
gt, lt, gte, lte, in, all, any, not
```

## Expression 
An expression is anything that returns a value. 

#### Literal values
| Example(s) | Description |
|------|-------------|
| "hello" | regular string |
| `123`, `0` | base-10 integer |
| `true`, `false` | boolean values |
| `100s`, `1m`, `1h` | time offset in seconds, minutes, and hours |

#### Common Operators
The following is a list of common operators and its precedence: 
| Operator | Description | Precedence | Example(s) |
|----------|-------------|------------|---------|
| ()       | function call/ grouping | 1        | `log.info()`, `(1 + 3) * 2` |
| .        | access      | 1 | `log.info`, `$user.name` |
| $        | event reference | 1 | `$user` |
| not      | logical NOT | 2 | `not 1 == 1` |
| -        | unary minus | 2 | `-5` |
| *        | multiplication | 3 | `5 * 3` |
| /        | division       | 3 | `5 / 3` |
| +     | addition | 4 | `5 + 3 ` |
| -     | substractiona | 4 | `5 - 3` |
| ..    | concat/range  | 4 | `"hello" .. "world"`, `10..15` |
| == | equality | 5 | `5 == 3` |
| and | logical AND | 6 | `5 == 5 and 1 == 1`  |
| or | logical OR | 7 | `5 == 2 or 1 == 1` |

## Comments
Jary only support single line comments using two backslash `//`:

```c
// This is a comment

// this
// is
// also a comment
```
