# llq (Live Log Query)

llq enables responsively quering your structured json logs to understand your application

<img src="https://github.com/user-attachments/assets/9e329a79-e398-4aac-abcf-236b93abce61" width="50%">
![out](https://github.com/user-attachments/assets/9e329a79-e398-4aac-abcf-236b93abce61 )

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
