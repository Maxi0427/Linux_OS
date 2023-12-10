/******************************************************************************
 *                                                                            *
 *                             Author: Hannah Pan                             *
 *                             Date:   04/15/2021                             *
 *                                                                            *
 ******************************************************************************/


#include "stress.h"

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>


/******************************************************************************
 *                                                                            *
 *  Replace kernel.h with your own header file(s) for p_spawn and p_waitpid.  *
 *                                                                            *
 ******************************************************************************/

#include "kernel.h"


static void nap(void)
{
  usleep(10000); // 10 milliseconds
  // p_sleep(1);/
  p_exit();
}


/*
 * The function below spawns 10 nappers named child_0 through child_9 and waits
 * on them. The wait is non-blocking if nohang is true, or blocking otherwise.
 */

static void spawn(bool nohang)
{
  char name[] = "child_0";
  char *argv[] = { name, NULL };
  int pid = 0;
  int c = 0;
  // Spawn 10 nappers named child_0 through child_9.
  for (int i = 0; i < 10; i++) {
    argv[0][sizeof name - 2] = '0' + i;
    const int id = p_spawn(nap, argv, 0, 1, true);

    if (i == 0)
      pid = id;

    dprintf(STDERR_FILENO, "%s was spawned\n", *argv);
  }
  // Wait on all children.
  while (1) {
    int status = -1;
    const int cpid = p_waitpid(-1, &status, nohang);
    dprintf(STDERR_FILENO, "wait returned: %d\n", cpid);
    if (cpid < 0)  // no more waitable children (if block-waiting) or error
      break;

    // polling if nonblocking wait and no waitable children yet
    if (nohang && cpid == 0) {
      usleep(90000);  // 90 milliseconds
      continue;
    }
    c++;
    dprintf(STDERR_FILENO, "child_%d was reaped\n", cpid - pid);
  }
  dprintf(STDERR_FILENO, "Total reaped: %d\n", c);
}

// static void spawn_2(bool nohang)
// {
//   char name[] = "child_0";
//   char *argv[] = { name, NULL };
//   int pid[10];

//   // Spawn 10 nappers named child_0 through child_9.
//   for (int i = 0; i < 10; i++) {
//     argv[0][sizeof name - 2] = '0' + i;
//     const int id = p_spawn(nap, argv, 0, 1, true);
//     pid[i] = id;
//     // if (i == 0)
//     //   pid = id;

//     dprintf(STDERR_FILENO, "%s was spawned\n", *argv);
//   }
//   // Wait on all children.
//   int i = 0;
//   while (i < 10) {
//     int status = -1;
//     const int cpid = p_waitpid(-1, &status, nohang);
//     i++;
//     if (cpid < 0)  // no more waitable children (if block-waiting) or error
//       break;
//     dprintf(STDERR_FILENO, "child_%d was reaped\n", cpid - pid[0]);
//   }
// }


/*
 * The function below recursively spawns itself 26 times and names the spawned
 * processes Gen_A through Gen_Z. Each process is block-waited by its parent.
 */

static void spawn_r(void)
{
  static int i = 0;
  int status = -1;
  int pid = 0;
  char name[] = "Gen_A";
  char *argv[] = { name, NULL };

  if (i < 26) {
    argv[0][sizeof name - 2] = 'A' + i++;
    pid = p_spawn(spawn_r, argv, 0, 1, true);
    dprintf(STDERR_FILENO, "%s was spawned\n", *argv);
    usleep(10000);  // 10 milliseconds
  }

  if (pid > 0 && pid == p_waitpid(pid, &status, false))
    dprintf(STDERR_FILENO, "%s was reaped\n", *argv);
  p_exit();
}


/******************************************************************************
 *                                                                            *
 * Add commands hang, nohang, and recur to the shell as built-in subroutines  *
 * which call the following functions, respectively.                          *
 *                                                                            *
 ******************************************************************************/

void hang(void)
{
  spawn(false);
}

void nohang(void)
{
  spawn(true);
}

void recur(void)
{
  spawn_r();
}
