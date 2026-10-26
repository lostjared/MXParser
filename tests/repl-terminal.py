import errno
import fcntl
import os
import pty
import re
import select
import subprocess
import sys
import termios
import time

master, slave = pty.openpty()

def setup_terminal():
    os.setsid()
    fcntl.ioctl(slave, termios.TIOCSCTTY, 0)

environment = dict(os.environ, INPUTRC=os.devnull, TERM="xterm")
process = subprocess.Popen([sys.argv[1], "--repl"], stdin=slave, stdout=slave,
                           stderr=slave, env=environment, preexec_fn=setup_terminal)
os.close(slave)
transcript = b""

def until(marker):
    global transcript
    received = b""
    deadline = time.monotonic() + 10
    while marker not in received:
        if time.monotonic() >= deadline:
            raise AssertionError(f"Terminal timeout waiting for {marker!r}: {transcript!r}")
        ready, _, _ = select.select([master], [], [], 0.1)
        if ready:
            try:
                data = os.read(master, 65536)
            except OSError as error:
                if error.errno != errno.EIO:
                    raise
                data = b""
            if not data:
                raise AssertionError(f"Terminal closed waiting for {marker!r}: {transcript!r}")
            received += data
            transcript += data
    return re.sub(rb"\x1b\[[0-?]*[ -/]*[@-~]", b"", received).replace(b"\r", b"")

try:
    assert b"ls-C Interactive" in until(b"$> ")
    os.write(master, b"1+2\n")
    assert b"\n3\n" in until(b"$> ")
    os.write(master, b"\x1b[A\n")
    assert b"1+2\n3\n" in until(b"$> ")
    os.write(master, b"12\x7f+3\n")
    assert b"\n4\n" in until(b"$> ")
    os.write(master, b"int twice(int n) {\n")
    until(b"... ")
    os.write(master, b"return n*2;\n")
    until(b"... ")
    os.write(master, b"}\n")
    until(b"... ")
    os.write(master, b"\n")
    until(b"$> ")
    os.write(master, b"twice(4)\n")
    assert b"\n8\n" in until(b"$> ")
    os.write(master, b"int total=0\n")
    until(b"$> ")
    os.write(master, b"for(int i=0;i<3;++i) {\n")
    until(b"... ")
    os.write(master, b"total+=twice(i);\n")
    until(b"... ")
    os.write(master, b"}\n")
    until(b"... ")
    os.write(master, b"if(0) {\n")
    until(b"... ")
    os.write(master, b"total=99;\n")
    until(b"... ")
    os.write(master, b"}\n")
    until(b"... ")
    os.write(master, b"else {\n")
    until(b"... ")
    os.write(master, b"total+=2;\n")
    until(b"... ")
    os.write(master, b"}\n")
    until(b"... ")
    os.write(master, b" \t\n")
    assert b"Error:" not in until(b"$> ")
    os.write(master, b"total\n")
    assert b"\n8\n" in until(b"$> ")
    os.write(master, b"(\n")
    until(b"... ")
    os.write(master, b"\x03")
    assert b"KeyboardInterrupt" in until(b"$> ")
    os.write(master, b"5+6\n")
    assert b"\n11\n" in until(b"$> ")
    os.write(master, b'total=9; lsprintf("running\\n"); while(1) { int local=0; ++local; }\n')
    until(b"running\r\n")
    os.write(master, b"\x03")
    response = until(b"$> ")
    assert b"KeyboardInterrupt" in response and b"instruction limit" not in response
    os.write(master, b"total\n")
    assert b"\n9\n" in until(b"$> ")
    os.write(master, b":reset\n")
    until(b"$> ")
    os.write(master, b'void spin(void) { lsprintf("running\\n"); while(1) {} }\n')
    until(b"$> ")
    os.write(master, b"spin()\n")
    until(b"running\r\n")
    os.write(master, b"\x03")
    assert b"KeyboardInterrupt" in until(b"$> ")
    os.write(master, b"5+6\n")
    assert b"\n11\n" in until(b"$> ")
    os.write(master, b"\x04")
    assert process.wait(timeout=10) == 0
except BaseException:
    sys.stderr.write(f"Terminal transcript: {transcript!r}\n")
    raise
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    os.close(master)
print("Readline prompts, history, editing, multiline input, Ctrl-C, and Ctrl-D verified")
