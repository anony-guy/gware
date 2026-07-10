# Gware Release v0.0.0.4

This release introduces a new native JSON parser alongside a native object structure representation, completing the goal of establishing versatile object syntax and networking capabilities in Gware!

## Changelog

* **Native JSON API**: 
  - `json_parse(string)`: Deserializes JSON formatted strings into Gware objects/arrays.
  - `json_stringify(object)`: Serializes Gware arrays and objects into a JSON formatted string.
* **Objects**: `ValueType` now officially supports `VAL_OBJECT` enabling hash-map/dictionary capabilities that act as first-class citizens in Gware!
  - You can parse complex nested JSON schemas into Gware and iterate or assign fields just like associative arrays: `data["key"] = value`.
* **String Parsing Fix**: The lexer was updated to parse escape characters, resolving earlier JSON string-parsing complications.

### Example Code

```gware
set str = "{\"name\":\"Gware\",\"version\":4,\"features\":[\"json\",\"net\"]}"
set data = json_parse(str)

show(data["name"]) // prints Gware

set data["language"] = "C"
show(json_stringify(data)) 
// prints: {"name":"Gware","version":4,"features":["json","net"],"language":"C"}
```
