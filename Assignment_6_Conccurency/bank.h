#ifndef _BANK_H
#define _BANK_H

#include <pthread.h>
#include <semaphore.h>

typedef struct Bank
{
  unsigned int numberBranches;
  struct Branch *branches;
  struct Report *report;
  pthread_mutex_t *branch_locks;
  pthread_mutex_t transfer_rep_lock;
  pthread_mutex_t report_lock;
  pthread_mutex_t report_compare_lock;
  sem_t starting_new;
  unsigned int num_workers, counter;
  
} Bank;

#include "account.h"

int Bank_Balance(Bank *bank, AccountAmount *balance);

Bank *Bank_Init(int numBranches, int numAccounts, AccountAmount initAmount,
                AccountAmount reportingAmount,
                int numWorkers);

int Bank_Validate(Bank *bank);
int Bank_Compare(Bank *bank1, Bank *bank2);

#endif /* _BANK_H */
