/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "treasury.h"

#include <stdio.h>

#include "gametime.h"
#include "log.h"
#include "treasury_repo.h"

#define TREASURY_MONTHLY_SPEND_PCT 95

/* -1 sentinel forces the first tick after boot to record a baseline month
 * rather than spending for whatever partial month was already in progress
 * (same shape as bank.c's last_seen_day). */
static long last_seen_month = -1;

int treasury_spend_monthly_improvements(void) {
    int bal = treasury_repo_get_gold();
    if (bal <= 0)
        return 0;
    int spend = (int)((long)bal * TREASURY_MONTHLY_SPEND_PCT / 100);
    if (spend <= 0)
        return 0;
    if (!treasury_repo_add_gold(-spend)) {
        log_error("treasury_spend_monthly_improvements: spend UPDATE failed");
        return 0;
    }
    char msg[256];
    snprintf(msg, sizeof(msg),
        "\r\n<Y>[Crown]<z> The crown allocates <c>%d<z> gold in collected taxes to public "
        "improvement projects, keeping <c>%d<z> gold in reserve.\r\n",
        spend, bal - spend);
    gametime_announce(msg);
    log_info("Improvement projects: spent %d of %d coffers gold (%d reserve).",
             spend, bal, bal - spend);
    return spend;
}

void treasury_monthly_tick(long pulse_num) {
    (void)pulse_num;
    long month = (long)gametime_year() * 12 + gametime_month();
    if (last_seen_month < 0) {
        last_seen_month = month;
        return;
    }
    if (month == last_seen_month)
        return;
    last_seen_month = month;
    treasury_spend_monthly_improvements();
}
