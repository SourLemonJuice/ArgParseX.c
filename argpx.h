#ifndef ARGPX_H_
#define ARGPX_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// the interface versions
#define ARGPX_VERSION_MAJOR 0
#define ARGPX_VERSION_MINOR 3
// a continuously growing integer, +1 for each release
#define ARGPX_VERSION_REVISION 3

// details are in ArgpxStatusToString()
enum ArgpxStatus {
    kArgpxStatusSuccess = 0,
    kArgpxStatusFailure,
    kArgpxStatusUnknownFlag,
    kArgpxStatusActionUnavailable,
    kArgpxStatusNoArgAvailableToShifting,
    kArgpxStatusParamNoNeeded,
    kArgpxStatusAssignmentDisallowAssigner,
    kArgpxStatusAssignmentDisallowTrailing,
    kArgpxStatusAssignmentDisallowArg,
    kArgpxStatusParamDisallowDelimiter,
    kArgpxStatusParamDisallowArg,
    kArgpxStatusParamDeficiency,
    kArgpxStatusGroupConfigEmptyString,
    kArgpxStatusParamExtraDelimiterAtTail,
};

enum ArgpxActionType {
    // get multiple flag parameters with different data type
    kArgpxActionParamMulti,
    // get a single flag parameter, but can still convert it's data type
    kArgpxActionParamSingle,
    // get flag parameters raw string array, the array size is dynamic
    // ParamList action only can uses the Delimiter but can't partition by arg
    kArgpxActionParamList,
    // if need some custom structure or the other data type
    kArgpxActionSetMemory,
    // the most common operation on the command line
    kArgpxActionSetBool,
    // like SetBool, maybe enum need it.
    // note: this action just uses the "int" type
    kArgpxActionSetInt,
};

enum ArgpxVarType {
    // string type will return a manually alloced full string(have \0)
    kArgpxVarString,
    // TODO
    kArgpxVarInt,
    kArgpxVarBool,
    kArgpxVarFloat,
    kArgpxVarDouble,
};

#define ARGPX_ATTR_ASSIGNMENT_DISABLE_ASSIGNER 0b1 << 0
// independent flag won't use trailing mode
#define ARGPX_ATTR_ASSIGNMENT_DISABLE_TRAILING 0b1 << 1
#define ARGPX_ATTR_ASSIGNMENT_DISABLE_ARG 0b1 << 2

// TODO preparing to discontinue
// #define ARGPX_ATTR_PARAM_DISABLE_DELIMITER 0b1 << 3
// #define ARGPX_ATTR_PARAM_DISABLE_ARG 0b1 << 4

#define ARGPX_ATTR_COMPOSABLE 0b1 << 5
#define ARGPX_ATTR_COMPOSABLE_NEED_PREFIX 0b1 << 6

struct ArgpxFlagGroup {
    // all group attribute
    uint16_t attribute;
    // prefix of flag, like the "--" of "--flag". it's a string
    // all group prefixes cannot be duplicated, including ""(single \0) also
    char *prefix;
    // flag parameter assignment symbol(string)
    // NULL: disable
    // the empty string: ""(single \0), is an error
    char *assigner;
    // flag parameter delimiter(string)
    // NULL: disable
    // the empty string: ""(single \0), is an error
    char *delimiter;
};

// Convert a string in flag's parameter
struct ArgpxParamUnit {
    enum ArgpxVarType type;
    // a list of secondary pointer of actual variable
    void *ptr;
};

struct ArgpxHidden_OutcomeParamMulti {
    int count;
    // format units array
    struct ArgpxParamUnit *units;
};

struct ArgpxHidden_OutcomeParamList {
    int *count;
    char ***params; // pointer to a string list...
};

struct ArgpxHidden_OutcomeSetMemory {
    size_t size;
    void *source_ptr;
    void *target_ptr;
};

struct ArgpxHidden_OutcomeSetBool {
    bool source;
    bool *target_ptr;
};

struct ArgpxHidden_OutcomeSetInt {
    int source;
    int *target_ptr;
};

// in library source code it is called "conf/config"
struct ArgpxFlag {
    // It's an index not an id
    // emm... I trust the programer can figure out the array index in their hands
    // then there is no need for a new hash table here
    int group_idx;
    // name of flag, like the "flagName" of "--flagName"
    char *name;
    // one flag only have one action, but one action may need to define mutiple structures.
    enum ArgpxActionType action_type;
    union {
        struct ArgpxParamUnit param_single;
        struct ArgpxHidden_OutcomeParamMulti param_multi;
        struct ArgpxHidden_OutcomeParamList param_list;
        struct ArgpxHidden_OutcomeSetMemory set_memory;
        struct ArgpxHidden_OutcomeSetBool set_bool;
        struct ArgpxHidden_OutcomeSetInt set_int;
    } action_load;
};

struct ArgpxResult {
    enum ArgpxStatus status;
    // index to the last parsed argument, processing maybe finished or maybe wrong
    int current_argv_idx;
    // pretty much the same as current_argv_idx, but it's directly useable string
    char *current_argv_ptr;
    // parameter here is non-flag command "argument"
    int param_count;
    // an array of command parameters
    char **paramv;
    // the command argc
    int argc;
    // the command argv
    char **argv;
};

enum ArgpxHidden_BuiltinGroup {
    kArgpxHidden_BuiltinGroupGnu,
    kArgpxHidden_BuiltinGroupUnix,
    kArgpxHidden_BuiltinGroupCount,
};

extern struct ArgpxFlagGroup argpx_hidden_builtin_group[kArgpxHidden_BuiltinGroupCount];

#define ARGPX_BUILTIN_GROUP_GNU argpx_hidden_builtin_group[kArgpxHidden_BuiltinGroupGnu]
#define ARGPX_BUILTIN_GROUP_UNIX argpx_hidden_builtin_group[kArgpxHidden_BuiltinGroupUnix]

/*
    The function parameter of ArgpxMain()
 */
struct ArgpxMainOption {
    int argc;
    char **argv;
    int argc_base;
    int groupc;
    struct ArgpxFlagGroup *groupv;
    int flagc;
    struct ArgpxFlag *flagv;
    void (*ErrorCallback)(struct ArgpxResult *);
};

char *ArgpxStatusToString(enum ArgpxStatus status);
struct ArgpxResult *ArgpxMain(struct ArgpxMainOption func_params);

#endif
