<div align="center"><img src="logo.png" height=50% width=50% /> </div>

## What is JARY?
JARY is a **standalone module** that can ingest normalized data and match it againts a `.jary` rule. The `.jary` rule is written in a syntax derived from the YARA language developed by VirusTotal. 

Here is an example of a `.jary` rule:  

```
ingress duty {
  field:
    person string
    task string
}

ingress person {
  field:
    name string
    age long
}

rule work_activity {
  match:
    $person.name join $duty.person
    $person.name exact "John Doe"
    $person.age between 9..30
    $duty.task exact "cleaning"
    $person within 10s

  output:
    $person.name .. " is working on " .. $duty.task .. " duty"
}
```

This rule essentially states that when there’s a person named John Doe, between the age of 9–30, who is on cleaning duty within the last 10 minutes, output the string "John Doe is working on cleaning duty".

## List of core features!
<details>
<summary>Data correlation engine in a single library!</summary>

Jary is a compact library, less than 150 KB, yet it provides both data correlation and rule creation.

As a library, it offers these advantages:
- No need for multiple processes to host the correlation engine.
- No network communication needed to send raw data to the engine.
- Everything is done through a simple function call!
</details>

<details>
<summary>Standardized rule definition!</summary>

Every rule follows a consistent, minimal format, without introducing too many concepts. A standard jary rule has this structure:

```txt
rule <name> {
  match:
  condition:
  output:
  action:
}
```

Once you understand these components, you grasp the flow of any jary rule. 

> Jary lets you focus on why a rule gets triggered and what action it will perform when triggered

</details>

<details>
<summary>Designed with flexibility in mind!</summary>

Jary aims to be the **the** [Lua](https://www.lua.org/) of embedded correlation engines. To achieve this, it is designed to accommodate the key requirements for analyzing and correlating diverse data.

Here’s how Jary addresses these core issues:
| Issue | Answer |
| ---   | ---    |
| Data can come from different sources by different providers | Users already have these data sources in their pipeline and understand their data, so they can normalize it and send it to Jary. |
| Different data patterns require unique analysis methods | Users can create custom Jary modules to analyze unique patterns, or they can use Jary for correlation and write the analysis logic as a callback. |
| Its not obvious if there is or isn't a correlation between data | It's never obvious; that’s why Jary provides the basic building blocks to write your own query logic, so it’s on you to find it. |
| Different respond for different data | Jary enables the user to define custom responses to specific data by creating custom actions in Jary or piping the output to a callback function. |
| Everyones data pipeline is unique | Jary is only a library, so it’s not limited to any pipeline. As long as the library can be compiled on that system, you can just use it. |


</details>

## Supported Platform
- GNU/Linux on AMD64

## Where can I use it?
JARY is a library that can be used in any case where automation is needed. For example, if you have a web server with multiple REST APIs, you can use JARY to protect it from brute force attacks and other security criteria. You simply need to write rules, such as tracking client IP addresses and creating rate limit rules for burst requests.


## How do I use it?
At the time of writing, JARY is provided as a `.so` file which exposes multiple functions for interfacing with the JARY library. This `.so` file must be linked to your main program using the method provided by the preferred language of your program. 

This could either mean creating a binding to the [ABI](https://en.wikipedia.org/wiki/Application_binary_interface), or your language can already talk to C. 

> In the future, I plan to provide an official binding for `Python`. In the meantime, only the C interface is available, as it's the language used to make this module. Read the [Get Started](https://github.com/CTRLRLTY/JARY/wiki/Get-Started) page to start using Jary.
>
> **NOTE:** I'd be happy to list any bindings you've created for `Jary` on this repository.

## What JARY tries to solve
The JARY module was created to help me automate data correlation from logs I've gathered on my home server. Unfortunately there were no tools that really met my needs. I've used `Yara` to perform malware analysis and it's really great, so why is there nothing similar for data correlation? That's where `Jary` comes in to fill that gap, and it must tackle the following concern:
1. A small dependency to anything, while providing everything.
2. The syntax that's either easy or natural enough to work with.
3. Fast where it must be fast.

### Problem domain - 1
If I need to only do `A`, then I only need `A`. If you need to do `B`, then you only need `B`. If we both want to be happy, then it must include `A` and `B`, right? **WRONG!**

If it provides `A` and `B` because someone need them, then later it has to provide `C` when someone else comes along. Give it time, and we'll not only have the entire Latin alphabet but Chinese characters too. Then I'd have to speak Chinese when I only need to type the latin character **A**. That would be toxic.

The solution should be given as a **choice** if and only if it is needed. It must not provide more than what you need while preserving your **freedom** of choice.

### Jary as a solution - 1a
Jary won't force you to include anything besides the base interpreter A.K.A the runtime VM to perform parsing, compiling and executing Jary's bytecode. This includes everything required to parse rules such as the one above. 

If you `import` any Jary module, you are essentially loading another `.so` file located somewhere on your filesystem. It’s not embedded in the language. For example:
```
// look for libsomelibrary.so on your LD PATH
import somelibrary
```

Anyone can create their own jary library by defining the `module_load` and `module_unload` functions in their `.so` file. Read [custom_module.md](https://github.com/CTRLRLTY/JARY/blob/0.x/doc/custom_module.md) to learn more. 
> You can use Jary without any imports. There's no standard libraries at the moment. I'll figure out what to add later. **Feedback needed**.

### Jary as a solution - 1b
`Jary` **does not** collect your log data for you, nor does it provide a way to read syslog, JSON, or any common data format. It is **your responsibility** to feed the data that will be handled by Jary. 

To clarify, let's imagine a scenario where you have a program `A` that fetches `JSON` data from a database: 
```json
{
  "user": "root",
  "activity": "failed login"
}
```
Program `A` must then feed this data in a way that Jary understands by using the functions provided by Jary:
```js
// Just a pseudo code...
//
// json data -> jary data -> jary
//

let jarydata = json_to_jary(jsondata)

send_to_jary("user_event", jarydata)
```
Once the data is ingested by Jary, it will be recognized as a `user_event` containing two fields:

```js
user_event: user = "root", activity = "failed login"
```
You can either feed more events or execute the `jary` rules to process the new event as it arrives.It’s crucial to understand that Jary does not work in the background on a separate thread or anything like that. It only works when you tell it to by calling the execute function:
```js
// pseudo code
execute(jary)
```
This is the core design of the library, which aims to stay out of the user’s way as much as possible by doing only what’s necessary.

### Problem domain - 2
If I am working on a ship with two other people and the three of us speak different languages such that person `A` speaks English, person `B` Italian, and I Klingon, what is the likelihood of me and them surviving if there's an iceberg up ahead? 

The answer would be **who knows!** And I'm not gonna wait to see myself in _Titanic 2_ to figure that out.

The same goes for writing an automated script. I do not want to figure out your native language just to see the script running `rm -rf` on my root directory.

The solution is to write in the same language, in the same way, in the same brain if we have to, or I won't allow your spooky code to fill a single bit on my hard drive.

### Jary as a solution - 2
Jary will dictate how you write your rules in a uniform way by forcing you to write the rules in one and only one way: 

```
rule your_rule {
  match:
  condition:
  output:
  action:
}
```

This yara-like rule structure will guide you on how to define your rules.   

> uniformity and conformity is the goal here, to prevent anyone from being too creative in their ways

#### Why borrow syntax from Yara?
The reasoning for this is simple: **I'm familiar with it** plus its popular among security folks and they are the target audience for this tool. 

> If the beauty/readibility of a language is subjective, just pick whichever that's more recognizable

#### Why not just use JSON or YAML?
If you thought "why not just use some small language like JSON or YAML to write the rules?" 

Well, have you actually spent a significant amount of time writing predicates in JSON/Yaml? It is TOXIC and UNPRODUCTIVE. 

Here's an example of what I meant:
```json
{
  "condition": {
    "compare": {
      "braincell.count": {
        "gte": 10
      }
    }
  },
  "action": {
    "delete-me-action": {
      "when": "soon"
    }
  }
}
```

compare it with the Jary rule:

```
rule somerule {
  condition:
    $braincell.count > 9

  action:
    delete_me("soon")
}
```

If you don't see a problem with the `JSON` rule, just go away.

### Problem domain - 3
The only reason for me to ever use a library or a service is that I'm either incapable of implementing the solution or I'm just too lazy enough to do it myself. Even with that premise, if the provided solution is too frustrating and always gets in my way, you can bet I'll make it myself. 

I don’t care if my home server is slower than Google’s kitchen toaster. I want the best of the best running on it. Every byte in its memory better not be polluted by inferior code that would diminish its ambition to be whatever it wants.

The solution must be fast for the sake of being fast, even if its domain is bottlenecked by frequent network I/O operations done by the main program, not by the library’s bad code. The user is entitled to their bad code, but the library is the responsibility of its author.

### Jary as a solution - 3
This is the first implementation of `JARY`, and not a lot of time has passed since its first inception (check the commit). In that short time, it already has a working [stack-based](https://en.wikipedia.org/wiki/Stack_machine) VM for running its bytecode, and the only dependency needed is `sqlite3` since it is the single best project that have ever lived. 

The language only have two phases to generate the bytecode: `parsing` and `compilation`. There's no optimizer nor a replacement for the default `malloc/free` allocation. But it can already compile the rule file like a champ.

> Measurement will be added in the future


## Additional Resource
- Get started: [Start writing Jary](https://github.com/CTRLRLTY/JARY/wiki/Get-Started)
- C API reference: [JARY/doc/c_api_reference.md](https://github.com/CTRLRLTY/JARY/blob/0.x/doc/c_api_reference.md)
- Rule reference: [JARY/doc/rule_reference.md](https://github.com/CTRLRLTY/JARY/blob/0.x/doc/rule_reference.md)
- Create a module: [JARY/doc/custom_module.md](https://github.com/CTRLRLTY/JARY/blob/0.x/doc/custom_module.md)

