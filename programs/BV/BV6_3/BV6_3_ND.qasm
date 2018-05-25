OPENQASM 2.0;
include "qelib1.inc";
qreg q[16];
creg c[16];
h q[1];
h q[2];
h q[12];
h q[14];
h q[4];
x q[13];
h q[1];
h q[2];
h q[13];
cx q[12], q[13];
h q[12];
h q[14];
h q[13];
cx q[13], q[14];
h q[14];
h q[13];
h q[14];
h q[4];
h q[13];
cx q[13], q[4];
h q[4];
h q[13];
h q[4];
h q[13];
measure q[12] -> c[0];
measure q[14] -> c[1];
measure q[4] -> c[2];
measure q[1] -> c[3];
measure q[2] -> c[4];
measure q[13] -> c[5];
