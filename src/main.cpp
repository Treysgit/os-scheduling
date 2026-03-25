#include <iostream>
#include <string>
#include <list>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <ncurses.h>
#include "configreader.h"
#include "process.h"

// Shared data for all cores
typedef struct SchedulerData {
    std::mutex queue_mutex;
    ScheduleAlgorithm algorithm; //1 of 4
    uint32_t context_switch; //time of switching processes on a core
    uint32_t time_slice; //fixed time-slice for RR
    std::list<Process*> ready_queue; //list of pointers to processes
    bool all_terminated; // indicate all processes are done
} SchedulerData;

void coreRunProcesses(uint8_t core_id, SchedulerData *data); // Used for dispatcher thread for each core
void printProcessOutput(std::vector<Process*>& processes); // table
std::string makeProgressString(double percent, uint32_t width); // progress bar in table
uint64_t currentTime(); // used for measuring elapsed time
std::string processStateToString(Process::State state); // convert the state (e.g., running) to a string

int main(int argc, char *argv[])
{
    // Ensure user entered a command line parameter for configuration file name
    if (argc < 2)
    {
        std::cerr << "Error: must specify configuration file" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Declare variables used throughout main
    int i;
    SchedulerData *shared_data = new SchedulerData(); // put new SchedulerData object on heap, return address for pointer shared_data
    std::vector<Process*> processes; // dynamic array for process pointers

    // Read configuration file for scheduling simulation
    SchedulerConfig *config = scr::readConfigFile(argv[1]);

    // Store number of cores in local variable for future access
    uint8_t num_cores = config->cores;

    // Store configuration parameters in shared data object
    shared_data->algorithm = config->algorithm;
    shared_data->context_switch = config->context_switch;
    shared_data->time_slice = config->time_slice;
    shared_data->all_terminated = false;


    // Create processes
    uint64_t start = currentTime();
    // loop through every process in config object
    for (i = 0; i < config->num_processes; i++)
    {
        Process *p = new Process(config->processes[i], start); // create a new process object for process i, p pointer to heap
        processes.push_back(p); //add pointer of new process to tail of vector 

        // If created process's start time is 0, it is ready to be put on ready-queue immidiately
        if (p->getState() == Process::State::Ready)
        {
            shared_data->ready_queue.push_back(p); // put process in struct ready queue
        }
        //vectors:
        // processes --> for keeping track of all processes 
        // ready-queue --> for when those processes have start time = elapsed (or re-entered)
    }

    // Free configuration data from memory
    scr::deleteConfig(config);

    // Launch 1 scheduling thread per cpu core
    std::thread *schedule_threads = new std::thread[num_cores]; // pointer for heap thread array. 1 dispatcher per core
    for (i = 0; i < num_cores; i++)
    {
        // create and run dispatcher threads for each core, store thread objects in array 
        schedule_threads[i] = std::thread(coreRunProcesses, i, shared_data);
    }

    // Main thread work goes here
    initscr();
    while (!(shared_data->all_terminated))
    {
        // Do the following:
        //   - Get current time
        //   - *Check if any processes need to move from NotStarted to Ready (based on elapsed time), and if so put that process in the ready queue
        //   - *Check if any processes have finished their I/O burst, and if so put that process back in the ready queue
        //   - *Check if any running process need to be interrupted (RR time slice expires or newly ready process has higher priority)
        //     - NOTE: ensure processes are inserted into the ready queue at the proper position based on algorithm
        //   - Determine if all processes are in the terminated state
        //   - * = accesses shared data (ready queue), so be sure to use proper synchronization

        uint64_t current_time = currentTime();
        uint64_t elapsed = current_time - start; // differnce of start of program and current time

        // Task 1: If time elapsed > the time before the process is available, it must be put in the ready-queue if it hasn't been.
        // Go through each process in vector
        for(int i = 0 ; i < processes.size() ; i++){

            Process *p = processes[i]; // pointer to process i
            if((p->getState() == Process::State::NotStarted) && (elapsed >= p->getStartTime())){

                // Set state to ready. Give current time 
                p->setState(Process::State::Ready, current_time); // launch_time = current_time

                if(shared_data->algorithm == ScheduleAlgorithm::SJF){
                    // helper function -- order based on shortest aggregate CPU time
                }
                else if(shared_data->algorithm == ScheduleAlgorithm::PP){
                    //helper function -- order based on priority field
                }
                else{
                    // FCFS and RR just send to the back of ready-queue
                    shared_data->ready_queue.push_back(p);
                }

            }


        }


        // Maybe simply print progress bar for all procs?
        printProcessOutput(processes);

        // sleep 50 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // clear outout
        erase();
    }


    // wait for threads to finish
    for (i = 0; i < num_cores; i++)
    {
        schedule_threads[i].join();
    }

    // print final statistics (use `printw()` for each print, and `refresh()` after all prints)
    //  - CPU utilization
    //  - Throughput
    //     - Average for first 50% of processes finished
    //     - Average for second 50% of processes finished
    //     - Overall average
    //  - Average turnaround time
    //  - Average waiting time


    // Clean up before quitting program
    processes.clear();
    endwin();

    return 0;
}

void coreRunProcesses(uint8_t core_id, SchedulerData *shared_data)
{
    // Work to be done by each core idependent of the other cores
    // Repeat until all processes in terminated state:
    //   - *Get process at front of ready queue
    //   - IF READY QUEUE WAS NOT EMPTY
    //    - Wait context switching load time
    //    - Simulate the processes running (i.e. sleep for short bits, e.g. 5 ms, and call the processes `updateProcess()` method)
    //      until one of the following:
    //      - CPU burst time has elapsed
    //      - Interrupted (RR time slice has elapsed or process preempted by higher priority process)
    //   - Place the process back in the appropriate queue
    //      - I/O queue if CPU burst finished (and process not finished) -- no actual queue, simply set state to IO
    //      - Terminated if CPU burst finished and no more bursts remain -- set state to Terminated
    //      - *Ready queue if interrupted (be sure to modify the CPU burst time to now reflect the remaining time)
    //   - Wait context switching save time
    //  - IF READY QUEUE WAS EMPTY
    //   - Wait short bit (i.e. sleep 5 ms)
    //  - * = accesses shared data (ready queue), so be sure to use proper synchronization
}

void printProcessOutput(std::vector<Process*>& processes)
{
    printw("|   PID | Priority |    State    | Core |               Progress               |\n"); // 36 chars for prog
    printw("+-------+----------+-------------+------+--------------------------------------+\n");
    for (int i = 0; i < processes.size(); i++)
    {
        if (processes[i]->getState() != Process::State::NotStarted)
        {
            uint16_t pid = processes[i]->getPid();
            uint8_t priority = processes[i]->getPriority();
            std::string process_state = processStateToString(processes[i]->getState());
            int8_t core = processes[i]->getCpuCore();
            std::string cpu_core = (core >= 0) ? std::to_string(core) : "--";
            double total_time = processes[i]->getTotalRunTime();
            double completed_time = total_time - processes[i]->getRemainingTime();
            std::string progress = makeProgressString(completed_time / total_time, 36);
            printw("| %5u | %8u | %11s | %4s | %36s |\n", pid, priority,
                   process_state.c_str(), cpu_core.c_str(), progress.c_str());
        }
    }
    refresh();
}

std::string makeProgressString(double percent, uint32_t width)
{
    uint32_t n_chars = percent * width;
    std::string progress_bar(n_chars, '#');
    progress_bar.resize(width, ' ');
    return progress_bar;
}

uint64_t currentTime()
{
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count();
    return ms;
}

std::string processStateToString(Process::State state)
{
    std::string str;
    switch (state)
    {
        case Process::State::NotStarted:
            str = "not started";
            break;
        case Process::State::Ready:
            str = "ready";
            break;
        case Process::State::Running:
            str = "running";
            break;
        case Process::State::IO:
            str = "i/o";
            break;
        case Process::State::Terminated:
            str = "terminated";
            break;
        default:
            str = "unknown";
            break;
    }
    return str;
}
