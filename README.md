## What is JARY?
JARY is a **standalone module** that can ingest normalized data and match it againts a `.jary` rule. The `.jary` rule is written in a syntax that is derived from the YARA language developed by VirusTotal. The following is an example of a `.jary` rule:  

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
The rule essentially states that when there's a person named John Doe between the age of 18-30 and is on a cleaning duty within the last 10 minutes, output the string "John Doe is working on cleaning duty".

## How do I use it?
At the time of writing, JARY is provided as a `.so` file which exposes multiple functions to interface with the JARY library. This `.so` file must then be link to your main program using the metode provided by the preferred language of your program. This means you have to create a binding to interface with the [ABI](https://en.wikipedia.org/wiki/Application_binary_interface). In the future I will provide an official binding for the `Python` language, in the meantime, only the C interface is usable as it is the language used to make this module. Read the [Get Started](https://github.com/CTRLRLTY/JARY/wiki/Get-Started) page to start using Jary.

> I'll be happy to list any bindings you've created for Jary on this repo.

## Use case
The JARY module was created to help me automate data correlation from logs I've gathered in my home server, and sadly there was no tools that really meet my needs. I've used `Yara` to perform malware analysis and it's really great, so why is there nothing similar for data correlation? That's where Jary's role comes to play by filling that vacuum, and it must tackle the following concern:
1. A small dependency to anything, while providing everything.
2. The syntax has to either be easy or natural enough to operate with.
3. Fast where it must be fast.

### Problem domain - 1
If I need to only do `A`, then I only need `A`. If you need to do `B`, then you only need `B`. If we both want to be happy, then it must include `A` and `B` right? **WRONG!**

If it provide `A` and `B` because someone need its, then later it has to provide `C` when someone else comes along. Give it time, and we'll not only have the entire latin alphabet but chinese characters too. Then I have to speak chinese when I only need to type the latin character **A**. That would be toxic.

The solution should be given as a **choice** if and only if it is needed. It must not provide more than what you need while preserving your **freedom** of choice.

### Jary as a solution - 1a
Jary won't force you to include anything besides the base interpreter A.K.A the runtime VM to perform parsing, compiling and executing Jary's bytecode. Meaning everything required to parse rules such as the one above. 

If perchance you `import` any jary module, you are essentially loading another `.so` library that is placed on somewhere your filesystem. It is not embedded in the language. Example:
```
// look for libsomelibrary.so on your LD PATH
import somelibrary
```

Anyone can create their own jary library by defining the `module_load` and `module_unload` functions in their `.so` file. Read \<insert link here\> to learn more. 
> You can use Jary without any import or barebone. There's no standard libraries at the moment, I'll figure out what to add later. **Feedback needed**.

### Jary as a solution - 1b
Jary **does not** care nor provide a way to collect your data log for you. It can not read syslog, JSON or any common data format. It is **your responsibility** to provide the data that will be handled by Jary. 

To make this clearer, let's imagine a scenario where you have a program `A` that fetch `JSON` data from a database: 
```json
{
  "user": "root",
  "activity": "failed login"
}
```
Program `A` must then feed this data in a way that Jary understand by using the functions provided by Jary:
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
You can either feed more events or execute the jary rules to process that new event entering. This is crucial to understand. Jary does not keep working on the background in a seperate thread or anything like that. It only works when you tell it to work by calling the execute function:
```js
execute_jary()
```
This is the principle design of the library, which aim to stay out of the way of the user as much as possible by not doing alot. 
### Problem domain - 2
The job of a computer language is to enable the programmer to vomit their creativity in written words. And there's a thing as being too creative. We can write rules in any computer language, like Python, but you'll find out later when you have to either read someone's code or having others read yours that it is always hard. 

I do not care how many years your experience is in whatever field, practice, or cult you are in, there is no such thing as clean code. Beauty is subjective and so is readibility of written logic. 

### Jary as a solution - 2
For the jary rules I chose a syntax that is popular in the malware analyst sphere, Yara. The reasoning for this is simple: **I'm familiar with it** plus its popular among security folks and they are the target audience for this tool. So if readibility or beauty of a language is subjective, just pick whichever is already common. That way that same people can just come right in.

Also for those who thinks "why not just use some small language like JSON or YAML to write the rules?" Well, have you actually tried writing conditional expression like `if/else` in JSON/Yaml? It is TOXIC and UNPRODUCTIVE. 

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
    $braincell.count >= 10

  action:
    delete_me("soon")
}
```
If you think the JSON rule is much better, go away.

## Additional Resource
Visit the project wiki [introduction](https://github.com/CTRLRLTY/JARY/wiki/) to learn more.

