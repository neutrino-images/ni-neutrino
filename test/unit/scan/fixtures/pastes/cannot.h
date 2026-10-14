/* The pastes settingsfield.h makes, none of which can spell a build condition. */
#define PASTE_FIELD(g, p) COREAPI_COLOR_GROUP_##g::p##_red
#define PASTE_ALPHA(has_alpha, g, p) COREAPI_COLOR_ALPHA_##has_alpha(g, p)
