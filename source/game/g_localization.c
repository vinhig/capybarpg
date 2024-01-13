#include <game/g_game.h>
#include <game/g_private.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void G_LoadTranslation(game_t *game, const char *path) {
  char base_path[256];
  sprintf(&base_path[0], "%s/%s", game->base, path);
  FILE *f = fopen(base_path, "r");

  if (!f) {
    printf("[ERROR] The localization file `%s`|`%s` doesn't seem to exist (skill "
           "issue).\n",
           path, base_path);
    return;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *content = calloc(size, sizeof(char));
  fread(content, 1, size, f);

  unsigned language_count = 0;
  unsigned language_capacity = 16;
  unsigned current_line = 0;
  unsigned cursor = 0;
  unsigned idx = 0;
  unsigned len = strlen(content);
  char *tmp_line = malloc(4096);
  char *tmp_entry = malloc(2048);

  while (idx <= len) {
    if (content[idx] == '\n' || idx == len) {
      strncpy(tmp_line, content + cursor, idx - cursor);
      tmp_line[idx - cursor] = '\0';

      // printf("got a line! %s \n", tmp_line);

      // first line contains language names, get the number of language
      if (cursor == 0) {
        unsigned line_len = strlen(tmp_line);
        for (unsigned j = 0; j < line_len; j++) {
          if (tmp_line[j] == ' ') {
            language_count++;
          }
        }

        game->translations = calloc(sizeof(char **), language_count);
        game->language_count = language_count;

        for (unsigned j = 0; j < language_count; j++) {
          game->translations[j] = calloc(sizeof(char *), language_capacity);
        }

        printf("[VERBOSE] Translation file `%s` contains %d languages.\n", path, language_count);
      }

      unsigned line_len = strlen(tmp_line);

      bool in_string = false;
      unsigned cursor_entry = 0;
      unsigned current_entry = 0;
      for (unsigned j = 0; j <= line_len; j++) {
        if (j == line_len || (tmp_line[j] == ' ' && !in_string)) {
          if (cursor_entry != 0) {
            // adding the translation entry here
            strncpy(tmp_entry, tmp_line + cursor_entry, j - cursor_entry);
            tmp_entry[j - cursor_entry] = '\0';
            // printf("\tentry is `%s`\n", tmp_entry);

            if (tmp_entry[0] == '"' && tmp_entry[strlen(tmp_entry) - 1] == '"') {
              game->translations[current_entry][current_line] = strncpy(calloc(strlen(tmp_entry), sizeof(char)), tmp_entry + 1, strlen(tmp_entry) - 2);
            } else {
              game->translations[current_entry][current_line] = strcpy(malloc(strlen(tmp_entry) + 1), tmp_entry);
            }

            current_entry++;
          }
          cursor_entry = j + 1;
        } else if (tmp_line[j] == '"') {
          in_string = !in_string;
        }
      }

      cursor = idx + 1;
      current_line++;

      if (current_line == language_capacity) {
        language_capacity *= 2;
        for (unsigned j = 0; j < language_count; j++) {
          game->translations[j] = realloc(game->translations[j], sizeof(char *) * language_capacity);
        }
      }
    }
    idx++;
  }

  game->entry_count = current_line;

  printf("ok loading done, let's see what's inside\n");

  for (unsigned i = 0; i < game->entry_count; i++) {
    printf("%d) %s == %s\n", i, game->translations[0][i], game->translations[1][i]);
  }

  free(tmp_line);
  free(tmp_entry);
}