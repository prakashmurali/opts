OPENQASM 2.0;
include "qelib1.inc";
qreg q[16];
creg c[16];
x q[15];
h q[2];
h q[3];
h q[1];
h q[3];
h q[1];
h q[15];
h q[2];
h q[15];
cx q[15], q[2];
h q[2];
h q[15];
h q[2];
h q[15];
measure q[2] -> c[0];
measure q[3] -> c[1];
measure q[1] -> c[2];
measure q[15] -> c[3];
