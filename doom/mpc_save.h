// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdio.h>

// Always owns and closes stream. Zero means flush, file fsync and close succeeded.
int mpc_save_close(FILE *stream, int write_failed);

// Publish a previously flushed temporary save and sync the destination directory.
// A failure after rename may mean final already contains the new save.
int mpc_save_replace(const char *temp, const char *final);
