# llq (Live Log Query)

llq enables responsively quering your structured json logs to understand your application
![llq-demo-1](https://github.com/user-attachments/assets/104d0901-4416-4a00-a41d-5bc3c31ca229)


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
