PUSH 1
PUSH 2
CALL subroutine
SET 101 @1
EQ 1 2
JF @1 label1
HOST
label1:
TERM

subroutine:
POP 1
POP 2
ADD @1 @2
SET 5 @1
SET 100 @1
SET 1 0
RET @5
