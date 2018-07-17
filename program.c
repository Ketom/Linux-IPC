#include <fcntl.h>
#include <signal.h> 
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/stat.h> // mkfifo()
#include <sys/wait.h>
#include <unistd.h>


/********************************************************************
 * Code below is bit ugly because of various rules i had to follow. *
 * Look in README for more info.                                    *
 ********************************************************************/


// #
// # Data types
// #

// object exchanged between processes to pass information
typedef struct {
    enum {MESSAGE_TYPE_PID, MESSAGE_TYPE_SIGNAL, MESSAGE_TYPE_WAIT} type;
    union {
        struct {
            int process_number;
            int pid_value;
        } pid;
        int signal;
        int wait;
    } content;
} Event;

// message used in message queue
typedef struct {
   long mtype;
   Event mdata;
} MsgqMessage;


// #
// # Constants
// #

#define FIFO_PERM 0600
#define LINE_MAX_LEN 512
#define MSGQ_PERM 0600
#define MSGQ_SIZE sizeof(Event)
#define PROCESS_COUNT 3
#define SIGNALS_COUNT 4
int const SIGNALS[SIGNALS_COUNT] = {
    SIGINT, // quit (end all actions and release resources)
    SIGTSTP, // pause
    SIGCONT, // resume
    SIGUSR1 // internal (inform other processes about state change)
    };
const char const *FIFO = "/tmp/linux_ipc_fifo";


// #
// # Global variables
// #

int id_msgq; // message queue identifier
int is_paused;
int is_waiting;
int is_terminated;
int pid_list[PROCESS_COUNT]; // PIDs of child processes


// #
// # Function declarations
// #

// helper functions to operate on message queue
int msgq_cleanup();
int msgq_create();
int msgq_get_key();
int msgq_send(int destination, Event event); // should not be used directly
int msgq_send_pid(int destination, int process_number, int pid_value);
int msgq_send_signal(int destination, int signal);
int msgq_send_wait(int destination, int wait);
void msgq_process_messages();

// functions called after message from queue have been processed
void received_pid(int process_number, int pid_value);
void received_signal(int signal);
void received_wait(int wait);

// function called after signal is received
void signal_handler(int signo);

// functions executed in child processes
int child_process0();
int child_process1();
int child_process2();

// common operations performed by all child processes
void child_process_init();

// function creating child process
// argument: function to be executed in child process
// return value: pid of child process
int create_child_process(int (*function)());

// custom implementation of strlen()
int my_str_len(const char const *str);


// #
// # Main
// #

int main() {
    int i, j;
    sigset_t set;

    // block all signals
    // otherwise process can exit before is should
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);

    // try to release resources if for some reason program exited unproperly before
    if (!msgq_cleanup()) {
        fprintf(stderr, "ERROR: Message queue already exists and it can't be removed.\n");
        return 1;
    }
    unlink(FIFO); // delete fifo if it exists

    // create objects used in child processes
    if (!msgq_create()) {
        fprintf(stderr, "ERROR: Unable to create message queue.\n");
        return 1;
    }
    mkfifo(FIFO, FIFO_PERM);

    printf("S1 = %d (end)\n", SIGNALS[0]);
    printf("S2 = %d (pause)\n", SIGNALS[1]);
    printf("S3 = %d (resume)\n", SIGNALS[2]);
    printf("S4 = %d (interal)\n", SIGNALS[3]);

    // set initial state
    is_paused = 0;
    is_waiting = 1;
    is_terminated = 0;
    for (i = 0; i < PROCESS_COUNT; i++) {
        pid_list[i] = 0;
    }

    pid_list[0] = create_child_process(child_process0);
    pid_list[1] = create_child_process(child_process1);
    pid_list[2] = create_child_process(child_process2);

    // send to each process all PIDs
    for (i = 0; i < PROCESS_COUNT; i++) {
        printf("pid_list[%d] = %d\n", i, pid_list[i]);
        for (j = 0; j < PROCESS_COUNT; j++) {
            msgq_send_pid(i, j, pid_list[j]);
        }
    }

    // tell process 0 to stop waiting
    msgq_send_wait(0, 0);

    // wait until all children end
    for (i = 0; i < PROCESS_COUNT; i++) {
        waitpid(pid_list[i], NULL, 0);
    }

    // release resources
    msgq_cleanup();
    unlink(FIFO);

    printf("\n");
    return 0;
}


// #
// # Function definitions
// #

int msgq_cleanup() {
    int key = msgq_get_key();
    id_msgq = -1;
    fprintf(stderr, "%d: msgq_cleanup()\n", getpid());
    if (key < 0) {
        return 1; // if we cant even get proper key, then our queue does not exists, right?
    }
    int id_msgq = msgget(key, 0);
    if (id_msgq < 0) {
        return 1; // queue does not exist, its ok
    }
    return (msgctl(id_msgq, IPC_RMID, NULL) >= 0);
}

int msgq_create() {
    int key;
    fprintf(stderr, "%d: msgq_create()\n", getpid());
    key = msgq_get_key();
    if (key < 0) {
        return 0;
    }
    id_msgq = msgget(key, MSGQ_PERM|IPC_CREAT);
    return (id_msgq >= 0);
}

int msgq_get_key() {
    int id = 1;
    char cwd[1024];
    fprintf(stderr, "%d: msgq_get_key()\n", getpid());
    if (getcwd(cwd, 1024) == NULL) {
        return -1;
    }
    return ftok(cwd, id);
}

int msgq_send(int destination, Event event) {
    MsgqMessage msg;
    int ret;
    //fprintf(stderr, "%d: msgq_send(...)\n", getpid());
    if (pid_list[destination] == 0) {
        return 0;
    }
    msg.mtype = pid_list[destination];
    msg.mdata = event;
    ret = msgsnd(id_msgq, &msg, MSGQ_SIZE, 0);
    kill(pid_list[destination], SIGNALS[3]);
    return (ret >= 0);
}

int msgq_send_pid(int destination, int process_number, int pid_value) {
    fprintf(stderr, "%d: msgq_send_pid(%d, %d, %d)\n", getpid(), destination, process_number, pid_value);
    Event event;
    event.type = MESSAGE_TYPE_PID;
    event.content.pid.process_number = process_number;
    event.content.pid.pid_value = pid_value;
    return msgq_send(destination, event);
}

int msgq_send_signal(int destination, int signal) {
    fprintf(stderr, "%d: msgq_send_signal(%d, %d)\n", getpid(), destination, signal);
    Event event;
    event.type = MESSAGE_TYPE_SIGNAL;
    event.content.signal = signal;
    return msgq_send(destination, event);
}

int msgq_send_wait(int destination, int wait) {
    fprintf(stderr, "%d: msgq_send_wait(%d, %d)\n", getpid(), destination, wait);
    Event event;
    event.type = MESSAGE_TYPE_WAIT;
    event.content.wait = wait;
    return msgq_send(destination, event);
}

void msgq_process_messages() {
    MsgqMessage msg;
    while (msgrcv(id_msgq, &msg, MSGQ_SIZE, getpid(), IPC_NOWAIT) >= 0) {
        if (msg.mdata.type == MESSAGE_TYPE_PID) {
            received_pid(msg.mdata.content.pid.process_number, msg.mdata.content.pid.pid_value);
        }
        if (msg.mdata.type == MESSAGE_TYPE_SIGNAL) {
            received_signal(msg.mdata.content.signal);
        }
        if (msg.mdata.type == MESSAGE_TYPE_WAIT) {
            received_wait(msg.mdata.content.wait);
        }
    }
}

void received_pid(int process_number, int pid_value) {
    fprintf(stderr, "%d: received_pid(%d, %d)\n", getpid(), process_number, pid_value);
    pid_list[process_number] = pid_value;
}

void received_signal(int signal) {
    fprintf(stderr, "%d: received_signal(%d)\n", getpid(), signal);
    if (signal == SIGNALS[0]) {
        is_terminated = 1;
        printf("%d: received S1!\n", getpid());
    }
    if (signal == SIGNALS[1]) {
        printf("%d: received S2!\n", getpid());
        is_paused = 1;
    }
    if (signal == SIGNALS[2]) {
        printf("%d: received S3!\n", getpid());
        is_paused = 0;
    }
}

void received_wait(int wait) {
    fprintf(stderr, "%d: received_wait(%d)\n", getpid(), wait);
    is_waiting = wait;
}

void signal_handler(int signo) {
    int i;
    fprintf(stderr, "%d: signal_handler(%d)\n", getpid(), signo);
    fflush(stdout);
    if (signo == SIGNALS[3]) {
        msgq_process_messages();
        return;
    }
    for (i = 0; i < PROCESS_COUNT; i++) {
        msgq_send_signal(i, signo);
    }
}

void child_process_init() {
    int i;
    sigset_t set;
    struct sigaction act;

    // register signals handler
    act.sa_handler = &signal_handler;
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    for (i = 0; i < SIGNALS_COUNT; i++) {
        sigaction(SIGNALS[i], &act, NULL);
    }

    // unblock signals we use
    sigemptyset(&set);
    for (i = 0; i < SIGNALS_COUNT; i++) {
        sigaddset(&set, SIGNALS[i]);
    }
    sigprocmask(SIG_UNBLOCK, &set, NULL);

    // wait until PIDs of all processes are received
    for (i = 0; i < PROCESS_COUNT; i++) {
        while (pid_list[i] == 0){
            //fprintf(stderr, "%d: pid_list[%d]=%d\n", getpid(), i, pid_list[i]);
            usleep(20 * 1000);
        }
    }
}

int child_process0() {
    char line_buffer[LINE_MAX_LEN];
    int fd_fifo_write = open(FIFO, O_WRONLY);

    fprintf(stderr, "%d: child_process0()\n", getpid());

    if (fd_fifo_write < 0) {
        return 1;
    }

    child_process_init();
    fprintf(stderr, "%d: child_process0 ready!\n", getpid());

    while (1) {
        while ((is_paused || is_waiting) && !is_terminated) {
            usleep(10 * 1000);
        }
        if (is_terminated) {
            break;
        }
        if (fgets(line_buffer, LINE_MAX_LEN, stdin) != NULL) {
            write(fd_fifo_write, line_buffer, LINE_MAX_LEN);
            is_waiting = 1;
            msgq_send_wait(1, 0);
        }
    }
    close(fd_fifo_write);
    return 0;
}

int child_process1() {
    char line_buffer[LINE_MAX_LEN];
    int fd_fifo_read = open(FIFO, O_RDONLY);
    int fd_fifo_write = open(FIFO, O_WRONLY);
    int line_len;

    fprintf(stderr, "%d: child_process1()\n", getpid());

    if (fd_fifo_read < 0 || fd_fifo_write < 0) {
        return 1;
    }

    child_process_init();
    fprintf(stderr, "%d: child_process1 ready!\n", getpid());

    while (1) {
        while ((is_paused || is_waiting) && !is_terminated) {
            usleep(10 * 1000);
        }
        if (is_terminated) {
            break;
        }

        if (read(fd_fifo_read, line_buffer, LINE_MAX_LEN) >= 0) {
            line_len = my_str_len(line_buffer);
            sprintf(line_buffer, "%d", line_len);
            write(fd_fifo_write, line_buffer, LINE_MAX_LEN);
            is_waiting = 1;
            msgq_send_wait(2, 0);
        }
    }
    close(fd_fifo_write);
    close(fd_fifo_read);
    return 0;
}

int child_process2() {
    char line_buffer[LINE_MAX_LEN];
    int fd_fifo_read = open(FIFO, O_RDONLY);
    int line_len;

    fprintf(stderr, "%d: child_process2()\n", getpid());

    if (fd_fifo_read < 0) {
        return 1;
    }

    child_process_init();
    fprintf(stderr, "%d: child_process2 ready!\n", getpid());

    while (1) {
        while ((is_paused || is_waiting) && !is_terminated) {
            usleep(10 * 1000);
        }
        if (is_terminated) {
            break;
        }

        if (read(fd_fifo_read, line_buffer, LINE_MAX_LEN) >= 0) {
            sscanf(line_buffer, "%d", &line_len);
            printf("%s\n", line_buffer);
            is_waiting = 1;
            msgq_send_wait(0, 0);
        }
    }
    close(fd_fifo_read);
    return 0;
}

int create_child_process(int (*function)()) {
    fprintf(stderr, "%d: create_child_process(...)\n", getpid());
    int pid = fork();
    if (pid == 0) {
        // child process
        exit(function());
        return 0; // never reached
    } else {
        // parent process
        return pid;
    }
}

int my_str_len(const char const *str) {
    const char *p = str;
    int i = 0;
    while (*p != '\0' && *p != '\n') {
        i++;
        p++;
    }
    return i;
}
