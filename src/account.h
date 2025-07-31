#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <time.h>
#include <stdbool.h>

#define MAX_CHARS_PER_ACCOUNT 10
#define MAX_SHARED_CHEST 100
#define ACCOUNT_NAME_LENGTH 32
#define ACCOUNT_PASS_LENGTH 128
#define ACCOUNT_EMAIL_LENGTH 64
#define ACCOUNT_FILE_PATH "../lib/accounts/"  // Ensure this exists

// Forward declaration for object data structure
struct obj_data;

// Structure representing an account with characters and shared inventory
struct account_data {
  char name[ACCOUNT_NAME_LENGTH];                         // Account username
  char password[ACCOUNT_PASS_LENGTH];                     // Hashed password
  char email[ACCOUNT_EMAIL_LENGTH];                       // Optional email
  char *characters[MAX_CHARS_PER_ACCOUNT];                // Character names
  int num_characters;                                     // Count of characters
  time_t created;                                         // Account creation time
  time_t last_login;                                      // Last login time
  bool siteok;                                            // Allowed from any site
  int max_online;                                         // Max simultaneous logins

  int shared_chest_slots;                                 // Max chest slots
  struct obj_data *shared_chest[MAX_SHARED_CHEST];        // Items in shared chest
};

// Forward declaration for descriptor
struct descriptor_data;

// Account lifecycle functions
struct account_data *create_account(const char *name, const char *password);
int save_account(struct account_data *account);
struct account_data *load_account(const char *name);
void free_account(struct account_data *account);

// Character management
int account_add_character(struct account_data *account, const char *charname);
int account_has_character(struct account_data *account, const char *charname);

// Menu / login helpers
void display_account_menu(struct descriptor_data *d);
int verify_password(struct account_data *account, const char *password);
void update_last_login(struct account_data *account);

#endif
