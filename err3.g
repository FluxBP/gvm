ADD 1 2
ADD @0 4
SUB SUB SUB SUB
SET 7 @0
JMP mylabel
SET 7 18446744073709551615
mylabel:
SET 2 10
SET 3 5
AND @2 @3
HOST
TERM
