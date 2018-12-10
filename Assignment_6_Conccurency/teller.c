#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"

#define BranchID uint64_t
/*
 * get the ID of the branch which the account is in.
 */
BranchID GetBranchID(AccountNumber accountNum)
{
  Y;
  return (BranchID)(accountNum >> 32);
}

/*
 * deposit money into an account
 */
int Teller_DoDeposit(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  if(amount == 0)
    return ERROR_SUCCESS;
    
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoDeposit(account 0x%" PRIx64 " amount %" PRId64 ")\n",
                accountNum, amount));


  Account *account = Account_LookupByNumber(bank, accountNum);

  if (account == NULL)
  {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  pthread_mutex_t *b_lock = &(bank->branch_locks[GetBranchID(accountNum)]);
  pthread_mutex_lock(&(account->lock));
  pthread_mutex_lock(b_lock);

  Account_Adjust(bank, account, amount, 1);

  pthread_mutex_unlock(b_lock);
  pthread_mutex_unlock(&(account->lock));

  return ERROR_SUCCESS;
}

/*
 * withdraw money from an account
 */
int Teller_DoWithdraw(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
  if (amount == 0)
    return ERROR_SUCCESS;

  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoWithdraw(account 0x%" PRIx64 " amount %" PRId64 ")\n",
                accountNum, amount));

  Account *account = Account_LookupByNumber(bank, accountNum);
  pthread_mutex_t *b_mtx = &(bank->branch_locks[GetBranchID(accountNum)]);

  if (account == NULL)
  {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  pthread_mutex_lock(&(account->lock));
  pthread_mutex_lock(b_mtx);

  if (amount > Account_Balance(account))
  {

    pthread_mutex_unlock(b_mtx);
    pthread_mutex_unlock(&(account->lock));

    return ERROR_INSUFFICIENT_FUNDS;
  }

  Account_Adjust(bank, account, -amount, 1);

  pthread_mutex_unlock(b_mtx);
  pthread_mutex_unlock(&(account->lock));

  return ERROR_SUCCESS;
}


int Teller_DoTransfer(Bank *bank, AccountNumber srcAccountNum,
                      AccountNumber dstAccountNum,
                      AccountAmount amount)
{
  assert(amount >= 0);

  DPRINTF('t', ("Teller_DoTransfer(src 0x%" PRIx64 ", dst 0x%" PRIx64
                ", amount %" PRId64 ")\n",
                srcAccountNum, dstAccountNum, amount));

  Account *srcAccount = Account_LookupByNumber(bank, srcAccountNum);
  if (srcAccount == NULL)
  {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  Account *dstAccount = Account_LookupByNumber(bank, dstAccountNum);

  if (dstAccount == NULL)
  {
    return ERROR_ACCOUNT_NOT_FOUND;
  }

  if (amount == 0 || srcAccountNum == dstAccountNum)
    return ERROR_SUCCESS;

  /*
   * If we are doing a transfer within the branch, we tell the Account module to
   * not bother updating the branch balance since the net change for the
   * branch is 0.
   */
  int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);
  BranchID src_br_id = GetBranchID(srcAccountNum);
  BranchID dst_br_id = GetBranchID(dstAccountNum);

  if (srcAccountNum < dstAccountNum)
  {
    pthread_mutex_lock(&(srcAccount->lock));
    pthread_mutex_lock(&(dstAccount->lock));
  }
  else
  {
    pthread_mutex_lock(&(dstAccount->lock));
    pthread_mutex_lock(&(srcAccount->lock));
  }

  if (amount > Account_Balance(srcAccount))
  {

    pthread_mutex_unlock(&(dstAccount->lock));
    pthread_mutex_unlock(&(srcAccount->lock));

    return ERROR_INSUFFICIENT_FUNDS;
  }

  if (updateBranch)
  {
    if (src_br_id < dst_br_id)
    {
      pthread_mutex_lock(&(bank->branch_locks[src_br_id]));
      pthread_mutex_lock(&(bank->branch_locks[dst_br_id]));
    }
    else
    {
      pthread_mutex_lock(&(bank->branch_locks[dst_br_id]));
      pthread_mutex_lock(&(bank->branch_locks[src_br_id]));
    }

    Account_Adjust(bank, srcAccount, -amount, updateBranch);
    Account_Adjust(bank, dstAccount, amount, updateBranch);

    pthread_mutex_unlock(&(bank->branch_locks[src_br_id]));
    pthread_mutex_unlock(&(bank->branch_locks[dst_br_id]));
  }
  else
  {
    Account_Adjust(bank, srcAccount, -amount, updateBranch);
    Account_Adjust(bank, dstAccount, amount, updateBranch);
  }

  pthread_mutex_unlock(&(dstAccount->lock));
  pthread_mutex_unlock(&(srcAccount->lock));

  return ERROR_SUCCESS;
}
