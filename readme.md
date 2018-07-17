# Linux IPC

Program using different [inter-process communication](https://en.wikipedia.org/wiki/Inter-process_communication) mechanisms available in Linux. It serves no real purpose.

This is my solution to assigment from "Operating systems" course. The task was to exchange data between three processes using named pipe (`mkfifo`). Synchronization had to be done with signals (`kill`) and message queue (`msgget`). User could send signal to any process requesting to *stop*, *pause* or *resume* communication. All processes had to react accordingly.

The task in fact was more elaborate and had few more restrictions, one of them being that all code should fit in one file. Because of those artificial restrictions, code is too complicated for any real life use. Therefore I decided to not refactor it, as writing desired program from scratch would be way faster. However, by solving that assigment I learned a lot about Linux IPC mechanisms: named pipes, message queues, signals and shared memory.

The original assigment text (in Polish) can be found in the `assigment-polish.md` file.
