/**
 * @mainpage Process Simulation
 *
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "proc_structs.h"
#include "proc_syntax.h"
#include "logger.h"
#include "manager.h"

#define LOWEST_PRIORITY INT_MAX // 0 is highest, setting INT_MAX as lowest
#define NOT_MAPPED -1 // if the process is not scheduled to run, assign this to pcb->rank

static pcb_queue_t terminatedq;  								//queue of complete processes
static pcb_queue_t waitingq;									//queue of waiting processes
static pcb_queue_t readyq;										//queue of avalible processes
static resource_t *system_resources;							//linked list of avalible processes

bool_t terminate();												//check if ready queue is empty
bool_t load_new_processes(void);								//check for new arrivial that needs to be added
void schedule_fcfs();											//schedule the process of first come first serve basis
void schedule_rr(int quantum);									//shedules in r-r fashion within given time quantum
void schedule_priority();										//shedule according to prioirty within long term sheduler

int execute_instr(pcb_t *proc);									//exectute pcb instructions
bool_t acquire_resource(pcb_t *proc, char *resource_name);		//get resources that have been requested
bool_t release_resource(pcb_t *proc, char *resource_name);		//frees resources

void enqueue(pcb_t *proc, pcb_queue_t *queue, int status);		//add back to spesific queue

/* get arguments */
int get_num_threads(int num_args, char **argv);					//numbers of cores needed
char *get_data(int num_args, char **argv);						//gets input data from file
int get_algo(int num_args, char **argv);						//determines if FCFS,RR or PRIORITY is used
int get_time_quantum(int num_args, char **argv);				//gets time quantum for rr
void print_args(int num_thr, char *data, int sched, int tq);	//print start up args
void print_queues(pcb_t *cur_pcb);								//prints contents of ready/waiting/terminated queues

int main(int argc, char** argv) {
  int num_thr = get_num_threads(argc, argv);
  char *data = get_data(argc, argv);
  int scheduler = get_algo(argc, argv);
  int time_quantum = get_time_quantum(argc, argv);
  print_args(num_thr, data, scheduler, time_quantum);
  bool_t success = FALSE;

  if (strcmp(data,"generate") == 0) {
	#ifdef DEBUG_MNGR
	printf("****Generate processes and initialise the system\n");
	#endif
	success = init_loader_from_generator();
  } else {
	#ifdef DEBUG_MNGR
	printf("Parse process file and initialise the system: %s \n", data);
	#endif
	success = init_loader_from_files(data);
  }

  if (success) {
	init_system();
	system_resources = get_resources();
	printf("***********Scheduling processes************\n");
	schedule_processes(num_thr, scheduler, time_quantum);
	dealloc_data_structures();
  } else {
	printf("Error: no processes to schedule\n");
  }

  return EXIT_SUCCESS;
}

/**
 * @brief The linked list of loaded processes is moved to the readyqueue.
 *	  The waiting and terminated queues are intialised to empty
 *
 *  TODO: Update the states of each process pcb added to readyq
 *  TODO: Update any counters used to detect termination
 *  TODO: Set readyq.last to point to the last pcb in the queue linked list
 */
void init_system(void)
{
	readyq.first = longterm_scheduler();
 	readyq.last = NULL;

	// TODO_LOGIC implementation
	pcb_t *current_node = readyq.first;
	while (current_node != NULL) {
		current_node->state = READY;

		// Updating / Moving the pointer of within the readyq
		readyq.last = current_node;
		current_node = current_node->next;

	}

 	waitingq.last = NULL;
	waitingq.first = NULL;
	terminatedq.last = NULL;
	terminatedq.first = NULL;

	print_queues(NULL);
}

/** @brief Schedules each instruction of each process */
void schedule_processes(int num_thr, schedule_t sched_type, int quantum)
{
  switch (sched_type) {
	case PRIOR:
	  schedule_priority();
	  break;
	case RR:
	  schedule_rr(quantum);
	  break;
	case FCFS:
	  schedule_fcfs();
	  break;
	default:
	  break;
  }
}

/** @brief Return true when there are no more processes to schedule
 * TODO: implement termination, ie. checking current status and updating
 * the waiting/termination counter to compair it to the total_jobs
 */
bool_t terminate() {
	if (readyq.first != NULL) {
		return FALSE;
	}

	int termination_counter = 0;
	int waiting_counter = 0;
	int totaljob_counter = 0;
	pcb_t *current_node;

	// Counting the number of processes that are blocked/waiting
	for (current_node = waitingq.first; current_node != NULL; current_node = current_node->next) {
		waiting_counter += 1;
	}

	// Counting the number of processes that have finished
	for (current_node = terminatedq.first; current_node != NULL; current_node = current_node->next) {
   		termination_counter += 1;
	}

	totaljob_counter = waiting_counter + termination_counter;
	if (totaljob_counter == get_total_jobs()) {
		return TRUE;
	}

	return FALSE;
}

/**
 * @brief Call the longterm schedule to check for new arrivals
 * TODO: Add new arrivals to the readyq using enqueue
 * TODO: and update any counters used to detect termination
 *
 * If there are new arrivals, call
 *	log_pcbs("New arrivals in ready queue", new_arrivals);
 */
bool_t load_new_processes(void) {
	bool_t added = FALSE;

	#pragma omp critical(readyq_lock)
	{
		pcb_t *new_arrivals = longterm_scheduler();
		// needs to handle 1 arrival at a time
		if (new_arrivals != NULL) {
			enqueue(new_arrivals, &readyq, READY);
			added = TRUE;
		}
	} //critical region
	return added;
}

/** Schedules processes using FCFS scheduling */
void schedule_fcfs(void) {
	// Loop until the a termination condition is met
	#pragma omp parallel
	{
		while (terminate() == FALSE) {
			pcb_t *current_pcb = NULL;

			//remove from readyq and set to running
			#pragma omp critical(readyq_lock)
			{
				load_new_processes();
				current_pcb = readyq.first;
				if (current_pcb != NULL) {
					readyq.first = current_pcb->next;
					if (readyq.first == NULL) {
						readyq.last = NULL;
					}
					current_pcb->next = NULL;
					current_pcb->state = RUNNING;
				}
			}

			if (current_pcb != NULL) {
				#pragma omp critical(print_lock)
				{
					log_running(current_pcb, omp_get_thread_num());
					print_queues(current_pcb);
				}

				//execute the instruction
				int status = RUNNING;
				while (status == RUNNING) {
					status = execute_instr(current_pcb);
				}

				#pragma omp critical(queue_lock)
				{
					//enqueuing into right space
					if (status == TERMINATED) {
						enqueue(current_pcb, &terminatedq, TERMINATED);
					} else if (status == WAITING) {
						enqueue(current_pcb, &waitingq, WAITING);
					}
				}
			}
		}
	} // parallel region
}

/** Schedules processes using the Round-Robin scheduler. */
void schedule_rr(int quantum) {
 	//continue until termination condition
	#pragma omp parallel
	{
		while (terminate() == FALSE) {
			pcb_t *current_pcb = NULL;

			//remove from readyq and set to running
			#pragma omp critical(readyq_lock)
			{
				load_new_processes();
				current_pcb = readyq.first;
				if (current_pcb != NULL) {
					readyq.first = current_pcb->next;
					if (readyq.first == NULL) {
						readyq.last = NULL;
					}
					current_pcb->next = NULL;
					current_pcb->state = RUNNING;
				}
			}

			if (current_pcb != NULL) {
				#pragma omp critical(print_lock)
				{
					log_running(current_pcb, omp_get_thread_num());
					print_queues(current_pcb);
				}

				int status = RUNNING;
				int instr_running = 0;

				//execute the instruction withing time_quantum of the R-R
				while (status == RUNNING && instr_running < quantum) {
					status = execute_instr(current_pcb);
			        instr_running += 1;

					//check for new arrivals after execting of existing -> important for scheduling
					load_new_processes();
				}

				#pragma omp critical(queue_lock)
				{
					//enqueuing into right space
					if (status == TERMINATED) {
    	   				enqueue(current_pcb, &terminatedq, TERMINATED);
			        } else if (status == WAITING) {
    	    			enqueue(current_pcb, &waitingq, WAITING);
	    	    	} else {
        				enqueue(current_pcb, &readyq, READY);
		      		}
				}
			}
		}
	} //parallel region
}

/** Schedules processes using priority scheduling with preemption
 * 	Kinda just use the logic from priorty queue within the context
 * 	of the scheduler.
 */
void schedule_priority(void) {
	#pragma omp parallel
	{
		//continue until termination condition
		while (terminate() == FALSE) {
			pcb_t *highest_priority = NULL;
			pcb_t *previous_highest = NULL;

			#pragma omp critical(readyq_lock)
			{
				load_new_processes();
				pcb_t *current = readyq.first;
				pcb_t *previous = NULL;
				int top_priority = INT_MAX;

				//Looping through the readyq to determine node of highest priority
				while (current != NULL) {
					if (current->priority < top_priority) {
						top_priority = current->priority;
						highest_priority = current;
						previous_highest = previous;
					}
					previous = current;
					current = current->next;
				}

				//removing the highest_p from queue and updating previous / pointers
				if (highest_priority != NULL) {
					if (previous_highest == NULL) {
						readyq.first = highest_priority->next;
					} else {
						previous_highest->next = highest_priority->next;
					}

					if (highest_priority == readyq.last) readyq.last = previous_highest;

					highest_priority->next = NULL;
					highest_priority->state = RUNNING;
				}
			}

			if (highest_priority != NULL) {
				#pragma omp critical(print_lock)
				{
					log_running(highest_priority, omp_get_num_threads());
					print_queues(highest_priority);
				}

				//execute the instruction and check if prioirity changes
				int status = RUNNING;
				while (status == RUNNING) {
					status = execute_instr(highest_priority);

					#pragma omp critical(readyq_lock)
					{
						bool_t arrival = load_new_processes();
						if (arrival == TRUE && status == READY) {
							pcb_t *temp = readyq.first;
							while (temp != NULL) {
								if (temp->priority < highest_priority->priority) {
									status = READY;
									break;
								}
								temp = temp->next;
							}
						}
					}
				}

				#pragma omp critical(queue_lock)
				{
					//enqueuing into right space
					if (status == TERMINATED) {
			    	    enqueue(highest_priority, &terminatedq, TERMINATED);
		        	} else if (status == WAITING) {
						enqueue(highest_priority, &waitingq, WAITING);
			        } else if (status == READY) {
    	    			enqueue(highest_priority, &readyq, READY);
					}
				}
			}
		}
	} // parallel region
}

/** Call the correct function to execute the next instruction of the process
 *	If there is an unknown / no instruction, call the appropriate log function:
 *	  log_unknown_instr() / log_no_instr()
 *	If the instruction was to release a resource, and it was successful,
 *	  wake up the first process in the waiting queue waiting for this resource
 *	  and if there is a process to wake up, log it with the log_wake_up() function
 *	Update the status of the process in its pcb and return its status,
 *	  so that the scheduler can act accordingly
 *
 **/
int execute_instr(pcb_t *pcb) {
	if (pcb->next_instruction == NULL) {
		log_no_instr(pcb->process->name);
		pcb->state = TERMINATED;
		return TERMINATED;
	}

	instr_t *next_instr = pcb->next_instruction;
	bool_t complete = TRUE;

	switch (next_instr->type) {
		// Trying to aquire the current resource, if not possible return FALSE
		case REQ_OP:
			complete = acquire_resource(pcb, next_instr->resource_name);
			break;

		// Trying to release the current resource, nevers stops till released
		case REL_OP:
			release_resource(pcb, next_instr->resource_name);
			complete = TRUE;
			break;

		// If current instruction dne, then call appopriote log
		default:
			log_unknown_instr(pcb->process->name);
			break;
	}

	// If instruction was aquired/released then move counter
	if (complete) {
		pcb->next_instruction = pcb->next_instruction->next;

		if (pcb->next_instruction == NULL) {
			pcb->state = TERMINATED;
			return TERMINATED;
		}
		return RUNNING;
	} else {
		return WAITING;
	}
}

/**
 * @brief Acquire a resource for a process if it is available
 *	   NB: Do not remove the resource from the system_resources list
 *	   Update the allocated field
 * If the resource was successfully acquired, the following log messages must be called:
 *	  log_request_acquired(cur_pcb->process->name, resource_name);
 *	  log_avail_resources(system_resources);
 *	  log_msg("\n");
 *
 */
bool_t acquire_resource(pcb_t *cur_pcb, char *resource_name) {

	bool_t success = FALSE;
	// Basically just checking while the current_resource is not empty, and it
	// equals resource in the pcb, then it logs the resource, else it returns
	// false
	#pragma omp critical(resources_lock)
	{
		resource_t *current_resource = system_resources;
		while (current_resource != NULL) {
			if (strcmp(current_resource->name, resource_name) == 0) {
				if (current_resource->allocated == NULL) {
					current_resource->allocated = cur_pcb;
					success = TRUE;

					log_request_acquired(cur_pcb->process->name, resource_name);
					log_avail_resources(system_resources);
					log_msg("\n");
				}
				break;
			}
			current_resource = current_resource->next;
		}
	} //critical region

  	return success;
}

/**
 * @brief Execute the release instruction for the process
 *	Update the allocated field
 *	Find a process that is waiting for a resource with the same name and move it to the ready queue
 *
 * If the release was successful, the following logging function must be called
 *	log_release_released(pcb->process->name, resource_name);
 *	log_avail_resources(system_resources);
 *	log_msg("\n");
 * If the release was unsuccessful, the following logging function must be called:
 *	log_release_error(pcb->process->name, resource_name);
 *
 */
bool_t release_resource(pcb_t *proc, char *resource_name) {
	// Searching for the current resource for the process
	bool_t found = FALSE;
	#pragma omp critical(resource_lock)
	{
		resource_t *current_resource = system_resources;
		while(current_resource != NULL) {
			if(strcmp(current_resource->name, resource_name) == 0 && current_resource->allocated == proc) {
				current_resource->allocated = NULL;

				log_release_released(proc->process->name, resource_name);
    	        log_avail_resources(system_resources);
        	    log_msg("\n");

				// Go through the qaiting queue to ensure that you find the 1st
				// process for the resource found
				#pragma omp critical(readq_lock)
				{
					pcb_t *previous = NULL;
					pcb_t *current = waitingq.first;

					while (current != NULL) {
						if (current->next_instruction != NULL && strcmp(current->next_instruction->resource_name, resource_name) == 0) {
							// take process from waiting queue
							if (previous == NULL) {
								waitingq.first = current->next;
							} else {
								previous->next = current->next;
							}

							if (current == waitingq.last) {
								waitingq.last = previous;
							}

							log_wake_up(current->process->name, resource_name);
							enqueue(current, &readyq, READY);
							break; // only taking the first process
						}
						previous = current;
						current = current->next;
					}
				} //critiacl region
				found = TRUE;
				break;
			}
			current_resource = current_resource->next;
		}
	} //critical region

	// If a resource was released by a process which doesn't own it -> error
	if (!found) {
		log_release_error(proc->process->name, resource_name);
	}
	return found;

}

/**
 * @brief Enqueue process <code>pcb</code> to <code>queue</code>
 * TODO Log the enqueue operation appropriately, depending on <code>status</code>
 *	 log_ready(pcb->process->name);
 *	 log_request_waiting(pcb->process->name, pcb->next_instruction->resource_name);
 *	 log_terminated(pcb->process->name);
 */
void enqueue(pcb_t *pcb, pcb_queue_t *queue, int status) {

	// Ensuring the pcb is not empty before adding to the queue
	if (pcb == NULL) {
		return;
	}

	// Making the next update value null before altering it and updating the
	// current state of the bus
	pcb->next = NULL;
	pcb->state = status;

	// Enqueing logic, ie. adding process to the back of the queue
	if (queue->first == NULL) {
		queue->first = pcb;
		queue->last = pcb;
	} else {
		queue->last->next = pcb;
		queue->last = pcb;
	}

	// Implementing the TODO_logic given
	if (status == READY) {
		log_ready(pcb->process->name);
	} else if (status == WAITING) {
		log_request_waiting(pcb->process->name, pcb->next_instruction->resource_name);
	} else if (status == TERMINATED) {
		log_terminated(pcb->process->name);
	}
}

/**
 * @brief detect deadlock
 * If deadlock is detected, the following log function must be called
 *	log_deadlock_detected();
 *
 *	basically just look for a cycle, if it exist deadlock exist thus starvation
 *	can occur, which we don't want
 */
struct pcb_t* detect_deadlock(void) {
  	pcb_t *start_node = waitingq.first;
	pcb_t *beginning;
	bool_t found_deadlock = FALSE;

	//loop through waiting queue to detect a possible cycle
	for (beginning = start_node; beginning != NULL; beginning = beginning->next) {
		pcb_t *current = beginning;
		//loop through all jobs and detect what resource the process is waiting for
		for (int i = 0; i < get_total_jobs(); i++) {
			if (current->next_instruction == NULL) break;

			char *target_resource_name = current->next_instruction->resource_name;
			resource_t *resource = system_resources;
			pcb_t *processor = NULL;

			//detect which process owns the resource we are looking for
			while (resource != NULL) {
				if (strcmp(resource->name, target_resource_name) == 0) {
					processor = resource->allocated;
					break;
				}
				resource = resource->next;
			}

			//no for deadlock
			if (processor == NULL) break;

			//yes for deadlock as resource is already owned by process we started with
			if (processor == beginning) {
				log_deadlock_detected();
				found_deadlock = TRUE;
				return beginning;
			}
			current = processor;
		}
	}

	//no deadlocks thus update the blocked log of processes
	if (!found_deadlock && waitingq.first != NULL) {
		log_blocked_procs();
	}
	return NULL;
}

/** @brief Deallocate the queues */
void free_manager(void) {
  print_queues(NULL);

  #ifdef DEBUG_MNGR
  printf("\nFreeing the queues...\n");
  #endif
  dealloc_pcbs(readyq.first);
  dealloc_pcbs(waitingq.first);
  dealloc_pcbs(terminatedq.first);
}

/** @brief Retrieve the number of threads to create from the list of arguments */
int get_num_threads(int num_args, char **argv) {
  if (num_args > 1) return atoi(argv[1]);
  else return 1;
}

/** @brief Retrieve the name of a process file or the codename "generate" from the list of arguments */
char *get_data(int num_args, char **argv) {
  char *data_origin = "generate";
  if (num_args > 2) return argv[2];
  else return data_origin;
}

/** @brief Retrieve the scheduler algorithm type from the list of arguments */
int get_algo(int num_args, char **argv) {
  if (num_args > 3) return atoi(argv[3]);
  else return 1;
}

/** @brief Retrieve the time quantum from the list of arguments */
int get_time_quantum(int num_args, char **argv) {
  if (num_args > 4) return atoi(argv[4]);
  else return 1;
}

/** @brief Print the arguments of the program */
void print_args(int num_thr, char *data, int sched, int tq) {
  printf("Arguments: num_threads = %d, data = %s, scheduler = %s,  time quantum = %d\n", num_thr, data, (sched==0)?"priority":(sched==1)?"RR":"FCFS", tq);
}


/**
 * @brief Print the currently running process, as well as all the queued processes
 */
void print_queues(pcb_t *cur_pcb) {
	#pragma omp critical(print_lock)
	{
		if (cur_pcb != NULL) log_running(cur_pcb, omp_get_thread_num());
		  log_queue(readyq.first, "Ready");
		  log_queue(waitingq.first, "Waiting");
		  log_queue(terminatedq.first, "Terminated");
		  log_msg("\n");
	}
}
