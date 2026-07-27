/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_BANK_H
#define TOBIN_BANK_H

/* Money system v2 (Sneezy → Tobin feature audit, "Money system v2
 * (banking/taxes)") -- bank interest. See cmd_bank.c for the
 * deposit/withdraw/balance commands and tobin_migrations.sql for the
 * scope-decision writeup (single global bank, not the original's
 * per-shop accounts + fractional-reserve central bank). */

#define BANK_INTEREST_RATE 0.005 /* 0.5% per in-game day */

/* Pulse callback (main.c, same ~60s cadence as gametime_tick()) -- checks
 * whether the in-game day (gametime_day()+month()+year(), not just the
 * 0-27 day-of-month) has rolled over since the last check, and if so
 * applies BANK_INTEREST_RATE to every player's bank_gold in a single SQL
 * UPDATE (not a per-online-character loop) so offline balances accrue
 * interest too, same as the original's daily interest job. */
void bank_interest_tick(long pulse_num);

#endif
