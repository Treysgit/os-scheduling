#include "process.h"




// Process class methods
Process::Process(ProcessDetails details, uint64_t current_time)
{
    int i;
    pid = details.pid;
    start_time = details.start_time;
    num_bursts = details.num_bursts;
    current_burst = 0;
    burst_times = new uint32_t[num_bursts];
    for (i = 0; i < num_bursts; i++)
    {
        burst_times[i] = details.burst_times[i];
    }
    priority = details.priority;
    state = (start_time == 0) ? State::Ready : State::NotStarted;
    if (state == State::Ready)
    {
        launch_time = current_time;
        burst_start_time = current_time;
    }
    is_interrupted = false;
    core = -1;
    turn_time = 0;
    wait_time = 0;
    cpu_time = 0;
    ready_enter_time = 0;
    total_time = 0;
    for (i = 0; i < num_bursts; i+=2)
    {
        total_time += burst_times[i];
    }
    remain_time = total_time;
}

Process::~Process()
{
    delete[] burst_times;
}

uint16_t Process::getPid() const
{
    return pid;
}

uint32_t Process::getStartTime() const
{
    return start_time;
}

uint8_t Process::getPriority() const
{
    return priority;
}

uint64_t Process::getBurstStartTime() const
{
    return burst_start_time;
}

// added for dispatcher
uint64_t Process::getBurstTime() const{
    return burst_times[current_burst];

}
// added for dispatcher
void Process::incrementBurst(){
    current_burst++; // index increment
}


Process::State Process::getState() const
{
    return state;
}

bool Process::isInterrupted() const
{
    return is_interrupted;
}

int8_t Process::getCpuCore() const
{
    return core;
}

double Process::getTurnaroundTime() const
{
    return (double)turn_time / 1000.0;
}

double Process::getWaitTime() const
{
    return (double)wait_time / 1000.0;
}

double Process::getCpuTime() const
{
    return (double)cpu_time / 1000.0;
}

double Process::getTotalRunTime() const
{
    return (double)total_time / 1000.0;
}

double Process::getRemainingTime() const
{
    return (double)remain_time / 1000.0;
}


void Process::setBurstStartTime(uint64_t current_time)
{
    burst_start_time = current_time;
}

void Process::setState(State new_state, uint64_t current_time)
{
    if (state == State::NotStarted && new_state == State::Ready)
    {
        launch_time = current_time;
    }
    state = new_state;
}

void Process::setCpuCore(int8_t core_num)
{
    core = core_num;
}

void Process::interrupt()
{
    is_interrupted = true;
}

void Process::interruptHandled()
{
    is_interrupted = false;
}

void Process::updateProcess(uint64_t current_time)
{
    
// `current_time` updates: turnaround time, wait time, burst times, cpu time, remaining time

// relevant members of process class:
	// state → NotStarted, Ready, Running, etc.
	// burst_start_time → the start or last update time of the current state
	// burst_times[i] → remaining time of each burst in respective process
	// current_burst → index in array of current burst of respective process
	//—————————————————————
	// wait_time → total time spent in Ready state
	// cpu_time → total time spent executing on CPU
	// turn_time → total time since initial Ready state until now
	// remain_time → remaining aggregate CPU time for respective process

    // exit if terminated or not yet in ready-queue
    if(state == State::NotStarted || state == State::Terminated){
        return;
    }

    //get the elapsed time since last update of process in state Ready, Running, or IO
    uint64_t elapsed = current_time - burst_start_time;
    //if no time passed since the last update, there's nothing to update
    if(elapsed == 0){
        return;
    }


    // Case 1: Running state update (CPU burst consumed)
    else if(state == State::Running){

        uint64_t consumed = elapsed;
        //amount of burst consumed can't be > amount left
        if(consumed > burst_times[current_burst]){
            //store actual amount consumed
            consumed = burst_times[current_burst];
        }
        
        burst_times[current_burst] -= consumed; //update burst with the amount consumed
        cpu_time += consumed; //update aggregate CPU time of process
        turn_time += consumed; //update aggregate time since entering ready-queue
        remain_time -= consumed; //decrease aggregate CPU time remaining for process
    }

    //Case 2: IO state update (IO burst consumed)
    else if(state == State::IO){
        // same logic as CPU burst 
        uint64_t consumed = elapsed; 
         if(consumed > burst_times[current_burst]){
            consumed = burst_times[current_burst];
        }
    
        burst_times[current_burst] -= consumed; //update burst with the amount consumed
        turn_time += consumed; //update aggregate time since entering ready-queue

    }

    // updates of elapsed time accounted for. Reset for next update
    burst_start_time = current_time;
}


void Process::setReadyEnterTime(uint64_t t){
    ready_enter_time = t;
}

uint64_t Process::getReadyEnterTime() const{
    return ready_enter_time;
}

void Process::addWaitTime(uint64_t delta){
    wait_time += delta;
}
    

    


void Process::updateBurstTime(int burst_idx, uint32_t new_time)
{
    burst_times[burst_idx] = new_time;
}
