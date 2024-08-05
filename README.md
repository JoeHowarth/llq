# llq (Live Log Query)

llq enables responsively quering your structured json logs to understand your application


## Query Syntax

- `msg`: Filters to logs that have the `msg` key and displays only the value of this key for each log
- `level == 'error', msg`: Filters to error logs with msg keys and displays both the level and msg 
- `level == 'error', *`: Similar to above, but the wildcard means the entire log object is displayed
- `sequence > 20, sequence < 30, msg`: Binary comparisons are implemented for integers

Queries are made of a comma separated list of expressions.
An expression contains a json path and optionally an operation and rhs
```
level
foo.bar.baz
array.5.key1
level == 'info'
count == 5
count > 5
count < 5
name > 'a'
*             # a wildcard is a special type of path specifier that matches all paths
```

## Example:

_log.json_
```json
{"level":"info","msg":"First log"}
{"level":"info","msg":"In the loop babyyy","count":5}
{"level":"info","msg":"Multiple of 5","count":5,"tag":"five"}
{"level":"info","msg":"Using some strings too","name":"Bobby 5"}
{"level":"info","msg":"In the loop babyyy","count":6}
{"level":"info","msg":"Even","count":6,"tag":"even"}
{"level":"info","msg":"Multiple of three","count":6,"tag":"three"}
{"level":"info","msg":"Using some strings too","name":"Bobby 6"}
{"level":"info","msg":"In the loop babyyy","count":7}
{"level":"info","msg":"Multiple of 7","count":7,"tag":"7"}
{"level":"info","msg":"Using some strings too","name":"Bobby 7"}
{"level":"info","msg":"In the loop babyyy","count":8}
{"level":"info","msg":"Even","count":8,"tag":"even"}
{"level":"info","msg":"Using some strings too","name":"Bobby 8"}
```

```sh
> llq log.json
```



```json

────────────────────────────────────────────────────────────────────
Query      :> 
Displaying :> 
```
