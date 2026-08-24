/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TREASURY_H
#define TOBIN_TREASURY_H

/* Monthly (game-time) treasury spend (user 2026-08-10): once per in-game
 * month, 95% of the crown's collected-tax coffers (treasury_repo -- fed by
 * shop sales tax and, minus the innkeeper's cut, rent tax) is spent on
 * public improvement projects and announced to every player. A gold sink
 * that keeps the tax pool from growing without bound. See treasury.c;
 * registered on a pulse in main.c, same cadence as bank_interest_tick. */

/* Pulse callback (main.c): detects a new in-game month and, when one
 * arrives, runs treasury_spend_monthly_improvements(). */
void treasury_monthly_tick(long pulse_num);

/* Spends 95% of the current coffers on improvement projects: subtracts it
 * from the treasury, broadcasts a "[Crown]" allocation line to every
 * player (gametime_announce), logs it, and returns the amount spent (0 if
 * the coffers were empty). Called monthly by the tick above and on demand
 * by `treasury allocate` (cmd_bank.c). */
int treasury_spend_monthly_improvements(void);

#endif
