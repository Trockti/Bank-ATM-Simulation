// OS-P3 2022-2023

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/stat.h>
#include <pthread.h>
#include "queue.h"
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_BUFFER 100
/**
 * Entry point
 * @param argc
 * @param argv
 * @return
 */
pthread_mutex_t mutex;	 // mutex to control access to shared buffer
pthread_cond_t no_full;	 // control if the buffer is not full
pthread_cond_t no_empty; // control if the buffer is not empty

char buffer[MAX_BUFFER]; // buffer to store operations
int number_elements;
int client_numop = 0;
int bank_numop = 0;
int global_balance = 0;
int *account_balance;
int maximum;
int max_accounts;
queue *q;
void *atm(void *arg)
{ // id producer

	char **list = (char **)arg;
	while (1)
	{

		pthread_mutex_lock(&mutex);
// Checks if the queue is full, if so, wait until it is not full
		while (queue_full(q))
		{ // if buffer full
			if (client_numop >= maximum)
			{

				pthread_cond_signal(&no_empty);
				pthread_mutex_unlock(&mutex);
				pthread_exit(0);
			}
			pthread_cond_wait(&no_full, &mutex); // blocked
	//If clients is bigger than maxiimun it exits the thread
		}
		if (client_numop >= maximum)
		{

			pthread_cond_signal(&no_empty);
			pthread_mutex_unlock(&mutex);
			pthread_exit(0);
		}
		char *op = list[client_numop];

		int i = 0;
		element *operation = malloc(sizeof(element));
		strcpy(operation->action, "");
		char *words[4];
		char *word = strtok(op, " ");
		while (word != NULL && i < 4)
		{
			words[i] = word;
			word = strtok(NULL, " ");
			i++;
		}

		strcpy(operation->action, words[0]);

		for (int i = 1; words[i] != NULL; i++)
		{

			operation->arguments[i - 1] = atoi(words[i]);
		}

		queue_put(q, operation);
		client_numop++;

		pthread_cond_signal(&no_empty); // signal buffer is no longer full
										// end critical secction
		pthread_mutex_unlock(&mutex);
	}
}
void *worker(void *kk)
{ // id consumer
	while (1)
	{

		pthread_mutex_lock(&mutex);
		// If queue is empty wait until something is introduce
		while (queue_empty(q))
		{ // if buffer empty
			if (bank_numop >= maximum)
			{
				;
				pthread_cond_signal(&no_full);
				pthread_mutex_unlock(&mutex);
				pthread_exit(0);
			}
			pthread_cond_wait(&no_empty, &mutex);
		}
		if (bank_numop >= maximum)
		{

			pthread_cond_signal(&no_full);
			pthread_mutex_unlock(&mutex);
			pthread_exit(0);
		}
		//Get the element from the queu
		element *op = queue_get(q);
		bank_numop++;
		//Takes the ammount of money 
		int account = op->arguments[0];
		if (strcmp(op->action, "CREATE") == 0)
		//If can not introduce more accounts
		{
			if (account > max_accounts){
				printf("This account can't be stored in our system");
				}
		// Prints the new balance
			else{
				account_balance[account - 1] = 0;
				printf("%d %s %d BALANCE=%d TOTAL=%d\n", bank_numop, op->action, account, account_balance[account - 1], global_balance);
				}
		}
		//Update the balances
		else if (strcmp(op->action, "DEPOSIT") == 0)
		{	// Checks if account exists
			if (account_balance[account - 1] == -1)
			{
				printf("You must have an account\n");
			}
			// Needs an input
			else if (op->arguments[1] <= 0)
			{
				printf("You must enter an amount of money\n");
			}
			// The update if all is correct
			else
			{
				account_balance[account - 1] += op->arguments[1];
				global_balance += op->arguments[1];
				printf("%d %s %d %d BALANCE=%d TOTAL=%d\n", bank_numop, op->action, account, op->arguments[1], account_balance[account - 1], global_balance);
			}
		}
		// Same as before but instead to deposit, to withdraw
		else if (strcmp(op->action, "WITHDRAW") == 0)
		{
			if (account_balance[account - 1] == -1)
			{
				printf("You must have an account\n");
			}
			if (op->arguments[1] <= 0)
			{
				printf("You must enter an amount of money\n");
			}

			else
			{
				account_balance[account - 1] -= op->arguments[1];
				global_balance -= op->arguments[1];
				printf("%d %s %d %d BALANCE=%d TOTAL=%d\n", bank_numop, op->action, account, op->arguments[1], account_balance[account - 1], global_balance);
			}
		}
		// Prints the balance of an account if the account exists
		else if (strcmp(op->action, "BALANCE") == 0)
		{
			if (account_balance[account - 1] == -1)
			{
				printf("You must have an account\n");
			}
			printf("%d %s %d BALANCE=%d TOTAL=%d\n", bank_numop, op->action, account, account_balance[account - 1], global_balance);
		}
		// Operation to tranfer from one account to other
		else if (strcmp(op->action, "TRANSFER") == 0)
		{
			//Checks if two account exists
			if (account_balance[account - 1] == -1 || account_balance[op->arguments[1] - 1] == -1)
			{
				printf("You must have an account\n");
			}
			//Checks if the argument exists
			if (op->arguments[2] <= 0)
			{
				printf("You didn't give money for the transfer\n");
			}
			else
			{
				//Checks if it is possible to do the opperation
				account_balance[account - 1] -= op->arguments[2];
				if (account_balance[account - 1] < 0)
				{
					account_balance[account - 1] += op->arguments[2];
					printf("You don't have enough money for the transfer\n");
				}
			}
			//Prints the result 
			account_balance[op->arguments[1] - 1] += op->arguments[2];

			printf("%d %s %d %d %d BALANCE=%d TOTAL=%d\n", bank_numop, op->action, account, op->arguments[1], op->arguments[2], account_balance[op->arguments[1] - 1], global_balance);
		}
		else
		{
			//If operation does not exists
			printf("Incorrect type of operation\n");
		}
		pthread_cond_signal(&no_full); // signal buffer is no longer full
		pthread_mutex_unlock(&mutex);
	}
}
int main(int argc, const char *argv[])
{
//Checks arguments
	if (argc != 6)
	{
		printf("The program must receive 5 arguments\n");
		exit(-1);
	}
//Checks if they are positive
	if (atoi(argv[2]) < 1 || atoi(argv[3]) < 1 || atoi(argv[4]) < 1 || atoi(argv[5]) < 1)
	{
		printf("The arguments must be positive number greater than 0");
		exit(-1);
	}
//Declares variables needed
	FILE *fd;
	int N = atoi(argv[2]);
	int M = atoi(argv[3]);
	pthread_t ATM[N];
	pthread_t workers[M];
// Check the file
	char max_operations[5];
	if ((fd = fopen(argv[1], "r")) == NULL)
	{
		perror("File does not exist\n");
		exit(-1);
	}
//Checks reading
	if (fgets(max_operations, 5, fd) == NULL)
	{
		perror("Error reading the line");
		return -1;
	}
//Checks opperations
	maximum = atoi(max_operations);
	if (maximum > 200)
	{
		printf("The maximum number of operations allowed is 200");
		return -1;
	}
	int i = 0;
	char **list_num_op = (char **)malloc(maximum * sizeof(char *));
	while (fgets(buffer, 100, fd) != NULL && i <= maximum)
	{
		list_num_op[i] = (char *)malloc((strlen(buffer) + 1) * sizeof(char));
		strcpy(list_num_op[i], buffer);
		i++;
	}
	if (i != maximum)
	{
		printf("There are not enough operations");
		exit(-1);
	}
	int lines = i - 1;
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&no_full, NULL);
	pthread_cond_init(&no_empty, NULL);
	//Creates the queue
	q = queue_init(atoi(argv[5]));
	
	max_accounts = atoi(argv[4]);
	account_balance = (int *)malloc(max_accounts * sizeof(int));
	//Create the accounts
	for (int i = 0; i < atoi(argv[4]); i++)
	{
		account_balance[i] = -1;
	}
	//Creates the ATMs
	for (int i = 0; i < N; i++)
	{
		pthread_create(&ATM[i], NULL, atm, (void *)list_num_op);
	}
	//Create the workers
	for (int i = 0; i < M; i++)
	{
		pthread_create(&workers[i], NULL, worker, NULL);
	}

	for (int i = 0; i < N; i++)
	{
		pthread_join(ATM[i], NULL);
	}

	for (int i = 0; i < M; i++)
	{
		pthread_join(workers[i], NULL);
	}
	
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&no_full);
	pthread_cond_destroy(&no_empty);
	//For liberating the memory
	for (int i = 0; i < maximum; i++)
	{
		free(list_num_op[i]);
	}
	free(list_num_op);
	free(account_balance);
	queue_destroy(q);

	exit(0);
}
