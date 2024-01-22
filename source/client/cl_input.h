#pragma once

#include <stdbool.h>
#include <wchar.h>

typedef struct client_t client_t;

/**
 * Specific key events aren't referenced. Only the meaning of them. For example,
 * it's not registered wether the player pressed 'W', only the fact that he/she
 * wants to go forward. The `client` component should take care of the specific
 * key bindings. Moreover, it abstracts the eventual joystick events that can be
 * analogic.
 */
typedef struct input_t {
  float zoom;
  // As in a joystick. The left pad allows to move gradually in both direction.
  struct {
    float x_axis;
    float y_axis;
  } movement;

  float wheel;

  // As in a joystick. The right pad allows to rotate the camera gradually in
  // both direction.
  struct {
    float x_axis;
    float y_axis;
  } view;

  struct {
    wchar_t content[128];
    bool submit;
  } text_editing;

  int mouse_x;
  int mouse_y;

  enum {
    __nothing = 0,
    __clicked = 1,
    __just_clicked = 2,
  } mouse_left, mouse_right;
} input_t;

input_t *CL_GetInput(client_t *client);
