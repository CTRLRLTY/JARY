## What is JARY?
JARY is a standalone module that can ingest normalized data and match it againts a `.jary` rule. The `.jary` rule is written in a syntax that is derived from the YARA language developed by VirusTotal. The following is an example of a `.jary` rule:  

```
import log

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

rule myrule
{
  match:
    $data.name join $duty.person
    $data.name exact "John Doe"
    $data.age between 18..30
    $duty.task exact "cleaning"
    $data within 10m

  action:
      log.info($data.name .. " is working on " .. $duty.task .. " duty")
}
```

The rule essentially states that when there's a person named John Doe between the age of 18-30 and is on a cleaning duty within the last 10 minutes, log the string "John Doe is working on cleaning duty".

## Additional resources
Visit the project wiki [introduction](https://github.com/CTRLRLTY/JARY/wiki/) to get started.

