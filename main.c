#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static const char *const savepath = "save.69";

// this is bad
#define IDLE_STS_MAX 1024
static char idle_status_impl[IDLE_STS_MAX];
static const char *idle_status = &idle_status_impl[0];

typedef struct IdleState {
  // Optional CRC of state to ensure it hasn't been altered.
  // TODO: Make points a bigint, or store a multiplier; something to make it seem bigger KEKW
  size_t points;
  // Often abbreviated PPS.
  size_t points_per_second;
  // Scaling factor applied to PPS when collecting points manually.
  size_t manual_scale;
  time_t time;
} IdleState;

void idle_save(IdleState *s) {
  FILE *f = fopen(savepath, "wb");
  fwrite(s, 1, sizeof(*s), f);
  fclose(f);
}

void idle_fprint(IdleState s, FILE *f) {
  fprintf(f,
          "POINTS:   %zu\n"
          "PPS:      %zu\n"
          "TIME:     %s\n",
          s.points,
          s.points_per_second,
          ctime(&s.time));
}

size_t idle_pps_cost(IdleState s) {
  // At first, the cost to upgrade pps should be slowish, and get faster quickly.
  // At some point, it should begin getting slower again (diminishing returns).
  // At some point after that, it should be ridiculously expensive.
  // TODO: This is not exactly ideal, but it's simple enoough and "works".
  // 120x^1.5 + 9;
  return s.points_per_second * (size_t)(s.points_per_second * 0.5) * 60 + 9;
}

size_t idle_manual_cost(IdleState s) {
  return s.manual_scale * (s.points_per_second * 0.5) * 120 + 10;
}

void idle_update(IdleState *s) {
  time_t current_time = time(NULL);
  double diff_seconds = difftime(current_time, s->time);
  s->time = current_time;
  size_t gained_points = s->points_per_second * diff_seconds;
  s->points += gained_points;
  snprintf(idle_status_impl, IDLE_STS_MAX, "Gained points: %zu", gained_points);
  idle_status = idle_status_impl;
}

typedef enum InputResult {
  InputInvalid,                //> call again (to hope) for valid input
  InputHandledContinue,        //> reenter user input loop
  InputHandledSaveAndContinue, //> Same as continue but saves first.
  InputHandledExit,            //> exit program after saving state
} InputResult;

InputResult handle_user_input(IdleState *state, char c) {
  const size_t cost_to_upgrade_pps = idle_pps_cost(*state);
  const size_t cost_to_upgrade_manual = idle_manual_cost(*state);
  switch (c) {
  default: break;

  case 'a': {
    state->points += state->points_per_second * state->manual_scale;
  } return InputHandledSaveAndContinue;

  case 'b': {
    idle_update(state);
    idle_save(state);

    if (state->points < cost_to_upgrade_pps) {
      // TODO: Print balance/cost...
      idle_status = "Not enough points to purchase upgrade.";
      return InputHandledContinue;
    }

    state->points -= cost_to_upgrade_pps;
    state->points_per_second += 1;
    snprintf(idle_status_impl, IDLE_STS_MAX,
             "Successfully increased points per second to %zu.",
             state->points_per_second);
    idle_status = idle_status_impl;
  }  return InputHandledContinue;

  case 'm': {
    idle_update(state);
    idle_save(state);

    if (state->points < cost_to_upgrade_manual) {
      idle_status = "Not enough points to purchase upgrade.";
      return InputHandledContinue;
    }

    state->points -= cost_to_upgrade_manual;
    state->manual_scale += 1;
    snprintf(idle_status_impl, IDLE_STS_MAX,
             "Successfully increased manual point collection scaling factor to %zu.",
             state->manual_scale);
    idle_status = idle_status_impl;
  }  return InputHandledContinue;

  case 'c': {
    idle_update(state);
    idle_save(state);
  } return InputHandledSaveAndContinue;

  case 'q': return InputHandledExit;

  }
  return false;
}

void user_input_loop(IdleState *state) {
  if (!state) return;
  // TODO: Make data driven with array/list of options or smth.
  for (;;) {
    printf("\033[2J"); //> Erase above cursor
    printf("\033[H");  //> Reset cursor position to row/column (1,1)

    if (idle_status) {
      printf("%s\n\n", idle_status);
      idle_status = NULL;
    }

    idle_fprint(*state, stdout);

    // TODO: Add manual collection upgrades.
    const size_t cost_to_upgrade_pps = idle_pps_cost(*state);
    const size_t cost_to_upgrade_manual = idle_manual_cost(*state);
    printf("\nOptions:\n"
           "  a :: Collect one iteration of points manually (earn %zu)\n"
           "  c :: Collect unclaimed points\n"
           "  b :: Buy an upgrade to increase points per second to %zu -- %zu\n"
           "  m :: Buy an upgrade to increase scaling factor on points earned manually to %zu -- %zu\n"
           "  q :: Save and exit; do nothing.\n"
           "> ",
           state->points_per_second * state->manual_scale,
           state->points_per_second + 1, cost_to_upgrade_pps,
           state->manual_scale + 1, cost_to_upgrade_manual
           );

    for (;;) {
      InputResult result = InputHandledContinue;
      result = handle_user_input(state, getchar());
      if (result == InputInvalid) continue;
      if (result == InputHandledSaveAndContinue) idle_save(state);
      if (result == InputHandledExit) return;
      break;
    }
  }
}

void restore_normal_buffer(void) {
    printf("\033[?1049l");
}

int main(void) {
  IdleState state = {0};
  FILE *f = NULL;

  //putchar('\n');

  f = fopen(savepath, "rb");
  if (!f) {
    // No save exists, create one.
    // Initialise state with the current time.
    state.points_per_second = 1;
    state.manual_scale = 1; //> TODO: Maybe should "unlock" this ability
    state.time = time(NULL);
    // Write default state to file.
    idle_save(&state);
    // Tell the user wtf just happened.
    idle_fprint(state, stdout);
    printf("\nNew save game created; run again to redeem points while idle.\n\n");
    return 0;
  }

  // Read save into state from file.
  fread(&state, 1, sizeof(state), f);
  fclose(f);

  idle_update(&state);

  printf("\033[?1049h");
  atexit(restore_normal_buffer);

  user_input_loop(&state);

  // Serialise state.
  idle_save(&state);

  return 0;
}
