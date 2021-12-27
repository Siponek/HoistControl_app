#define true 1
#define false 0
//! Variables to change with signals

float resetSpeed = 0;
int fileDescriptorLog;
int fileDescriptorErrorLog;
extern float resetSpeed;

#include "functionsFile.h"

pid_t createMotorProcess(char axis);
// void sigHandlerReset(int signum);
void motorProcess(char axis, int fileDescriptorErrorLog, int fileDescriptorLog);
void sendToMotor(int fileDescriptor, float speed);

int main(int argc, char const *argv[])
{
    makeFolder("logs");
    /// Directory for pipes.
    makeFolder("communication");
    makeFolder("tmp");

    fileDescriptorLog = createFile("logs/logs.txt");
    fileDescriptorErrorLog = createFile("logs/errorsLogs.txt");

    logWrite(fileDescriptorLog, "Master: Start\n;");

    /// From master to motors communication.
    createFIFO("communication/motorProcess_X");
    createFIFO("communication/motorProcess_Z");

    /// From motors to master communication.
    createFIFO("communication/motorProcessConsole_X");
    createFIFO("communication/motorProcessConsole_Z");

    pid_t pidMotorX = createMotorProcess('X');
    pid_t pidMotorZ = createMotorProcess('Z');

    yellow();
    printf("Parent: Running...\n");
    reset();

    configTerminal();

    int fileDescriptorX = open("communication/motorProcess_X", O_WRONLY);
    if (fileDescriptorX == -1)
    {
        printf("Could not open FIFO for motor X file\n");
        exit(1);
    }

    int fileDescriptorZ = open("communication/motorProcess_Z", O_WRONLY);
    if (fileDescriptorZ == -1)
    {
        printf("Could not open FIFO for motor Z file\n");
        exit(1);
    }

    writeCurrentProcessPIDToFile("tmp/PID_masterProcess");
    int userAction;
    while (1)
    {
        fflush(stdin);
        yellow();
        printf("W - Moving in Z axis UP\nS - Moving in Z axis DOWN\nA - Moving in X axis LEFT\nD - moving in X axis RIGHT\n");
        reset();
        printf("Parent: Waiting for input\n");
        userAction = getchar();

        if (userAction == (int)'a')
        {
            printf("You typed in %c !\n", userAction);
            sendToMotor(fileDescriptorX, (float)10);
            logWrite(fileDescriptorLog, "Master: X++\n");
        }
        else if (userAction == (int)'d')
        {
            printf("You typed in %c !\n", userAction);
            sendToMotor(fileDescriptorX, (float)-10);
            logWrite(fileDescriptorLog, "Master: X--\n");
        }
        else if (userAction == (int)'w')
        {
            printf("You typed in %c !\n", userAction);
            sendToMotor(fileDescriptorZ, (float)20);
            logWrite(fileDescriptorLog, "Master: Z++\n");
        }
        else if (userAction == (int)'s')
        {
            printf("You typed in %c !\n", userAction);
            sendToMotor(fileDescriptorZ, (float)-20);
            logWrite(fileDescriptorLog, "Master: Z--\n");
        }
        else if (userAction == (int)'t')
        {
            red();
            printf("You typed in %c , THE RESET BUTTON!\n", userAction);
            reset();

            // sendToMotor(fileDescriptorZ, (float)0);
            // sendToMotor(fileDescriptorX, (float)0);
            kill(pidMotorZ, SIGUSR1);
            kill(pidMotorX, SIGUSR1);

            logWrite(fileDescriptorLog, "Master: RESET BUTTON\n");
        }
        else
        {
            printf("Waiting for input...\n");
        }

        usleep(100000);
    }

    // pid_t processID = fork();

    /*fileDescriptor[0] is for READ, fileDescriptor[1] is for WRITE*/
    // mkfifo("myfifo1", 0777); // A classic mkfifo

    /*Waiting for ALL CHILD processes to finish*/
    while (wait(NULL) != -1 || errno != ECHILD)
        ;
    // // getting file descriptors for fifo files in communication folder

    return 0;
}

pid_t createMotorProcess(char axis)
{
    char watchdogPIPEPath[50] = "communication/pid_Motor";
    char helperChar[2];
    helperChar[0] = axis;
    helperChar[1] = '\0';

    strcat(watchdogPIPEPath, helperChar);

    pid_t pidMotor = fork();

    if (pidMotor == -1)
    {
        printf("Failed to fork motor process %c\n", axis);
        logWrite(fileDescriptorErrorLog, "Master: Failed to fork motor process. \n"); // TODO add axis letter
        exit(1);
    }
    else if (pidMotor == 0)
    {
        //! Body of Motor process.

        char pathForPID[50] = "tmp/PID_motor_";
        char helperChar[2];
        helperChar[0] = axis;
        helperChar[1] = '\0';
        strcat(pathForPID, helperChar);

        writeCurrentProcessPIDToFile(pathForPID);
        // watchdogPID_Write(watchdogPIPEPath, false, fileDescriptorErrorLog);
        yellow();
        printf("Child: Motor %c running...\n", axis);
        reset();
        motorProcess(axis, fileDescriptorErrorLog, fileDescriptorLog);
    }
    else
    {
        // Master process.
        printf("Child%c: Writing PID to watchdog\n", axis);
        logWrite(fileDescriptorLog, "Master: Created a child Motor\n;"); // TODO add axis letter

        // So we get an error in that functions, bcs we return a value only here,
        // and not in previous `else` cases. But it's fine, bcs
        // we need to return pidMotor for parent process.
        return pidMotor;
    }
}

// void sigHandlerReset(int signum)
// {
//     if (signum = SIGUSR1)
//     { // Return type of the handler function should be void
//         printf("\nMotors registered a RESET button\n");

//         printf("Child: Motor RESET\n");
//         sendToMotor(fileDescriptorConsole, 0);
//     }
// }

void motorProcess(char axis, int fileDescriptorErrorLog, int fileDescriptorLog)
{
    logWrite(fileDescriptorLog, "Child Motor: Start\n;"); // TODO letter for axis
    srand((unsigned)time(NULL));                          // Set random seed.
    char motorProcessPath[50] = "communication/motorProcess_";
    char motorProcessConsolePath[50] = "communication/motorProcessConsole_";
    char helperChar[2];

    helperChar[0] = axis;
    helperChar[1] = '\0';

    strcat(motorProcessPath, helperChar);
    strcat(motorProcessConsolePath, helperChar);

    printf("Child: %c Opening a pipe masterToChild\n", axis);

    int fileDescriptorMaster = open(motorProcessPath, O_RDONLY);
    if (fileDescriptorMaster == -1)
    {
        printf("Could not open FIFO master-child file\n");
        exit(1);
    }

    printf("Child: %c Opening a pipe childToConsole\n", axis);

    int fileDescriptorConsole = open(motorProcessConsolePath, O_WRONLY);
    if (fileDescriptorConsole == -1)
    {
        printf("Could not open FIFO console file\n");
        exit(1);
    }

    int randomness;
    float currentError = 0;
    float currentState = 0;
    float speed;
    while (1)
    {

        //*A random error with motor process.
        currentError = ((float)rand() / (RAND_MAX)) / 10;
        randomness = rand() % 10;
        if (read(fileDescriptorMaster, &speed, sizeof(speed)) == -1)
        {
            if (errno == EAGAIN)
            {
                printf("Pipe is empty\n");
            }
            else
            {
                printf("Could not read FIFO file\n");
                logWrite(fileDescriptorErrorLog, "Motor: Could not open WR FIFO file\n");
                exit(1);
            }
        }

        randomness = rand() % 10;
        if (randomness >= 5)
        {
            currentState = speed + currentError;
        }
        else
        {
            currentState = speed - currentError;
        }

        // to allow to pass the next if(), if the signal is detected then the fucntion
        // changes resetSpeed to 0 and resets the engine
        // printf("Child: Motor%c speed -> %f m/s \n", axis, currentState);
        resetSpeed = currentState;

        // signal(SIGUSR1, sigHandlerReset); // TODO

        if (currentState == resetSpeed)
        {
            sendToMotor(fileDescriptorConsole, currentState);
        }
        usleep(400000);
    }
    close(fileDescriptorMaster);
}

void sendToMotor(int fileDescriptor, float speed)
{
    if (write(fileDescriptor, &speed, sizeof(speed)) == -1)
    {
        printf("Could not write to motor-pipe. Error -> %d\n", errno);
        fflush(stdout);
        logWrite(fileDescriptorErrorLog, "sendToMotor -> failed to write into a pipe");
        exit(1);
    }
    fflush(stdout);
}
