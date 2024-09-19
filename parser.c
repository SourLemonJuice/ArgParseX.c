#include "parser.h"

#include <iso646.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
    An unified data of this library
 */
struct UnifiedData_ {
    // the result structure of the main function
    struct ArgpxResult *res;
    // error callback function pointer, NULL is exit(EXIT_FAILURE)
    void (*ErrorCallback)(struct ArgpxResult *);
    // arguments count
    int arg_c;
    // pointer to arguments array
    char **args;
    // the arg index being processed, it records the first unprocessed arg.
    // no no no, try to make it to record the last processed arg
    int arg_idx;
    // groups count
    int group_c;
    // pointer to groups array.
    struct ArgpxFlagGroup *groups;
    // configs count
    int conf_c;
    // pointer to configs array
    struct ArgpxFlag *confs;
};

/*
    Call the error callback registered by the user
 */
static void ArgpxExit_(struct UnifiedData_ data[static 1], enum ArgpxStatus status)
{
    data->res->status = status;

    if (data->ErrorCallback != NULL)
        data->ErrorCallback(data->res);

    exit(EXIT_FAILURE);
}

/*
    Convert structure ArgpxResult's ArgpxStatus enum to string
 */
char *ArgpxStatusToString(enum ArgpxStatus status)
{
    switch (status) {
    case kArgpxStatusSuccess:
        return "Processing success";
        break;
    case kArgpxStatusFailure:
        return "Generic unknown error";
        break;
    case kArgpxStatusActionUnavailable:
        return "Flag action availability error. Maybe that's not available in the current setup";
        break;
    case kArgpxStatusShiftingArg:
        return "There are no more arguments to get";
        break;
    case kArgpxStatusUnknownFlag:
        return "Unknown flag but the flag group matched(by prefix)";
        break;
    case kArgpxStatusRequiredAssigner:
        return "Assignment symbol(assigner) is not detected, but flag group set it to be mandatory";
        break;
    case kArgpxStatusRequiredDelimiter:
        return "Delimiter is not detected, but flag group set it to be mandatory";
        break;
    case kArgpxStatusFlagParamDeficiency:
        return "The flag gets insufficient parameters";
        break;
    }

    return NULL;
}

/*
    Using the offset shift arguments, it will be safe.
    Return a pointer to the new argument
 */
static char *ShiftArguments_(struct UnifiedData_ data[static 1], int offset)
{
    if (data->arg_idx + offset >= data->arg_c)
        ArgpxExit_(data, kArgpxStatusShiftingArg);
    data->arg_idx += offset;

    return data->args[data->arg_idx];
}

/*
    Convert group index to pointer.
    Some special index(negative number) will be resolved to built-in group

    return NULL is non-flag argument
 */
static struct ArgpxFlagGroup *GroupIndexToPointer_(struct UnifiedData_ data[static 1], int index)
{
    if (index == INT_MIN)
        return NULL;

    struct ArgpxFlagGroup *result = NULL;
    if (index >= 0) {
        result = &data->groups[index];
    } else {
        // TODO
    }

    return result;
}

/*
    Copy the current argument to result data structure as a command parameter
 */
static void AppendCommandParameter_(struct UnifiedData_ data[static 1])
{
    char *arg = data->args[data->arg_idx];
    struct ArgpxResult *res = data->res;

    res->params_count += 1;
    size_t this_arg_size = strlen(arg) + 1;

    res->params = realloc(res->params, sizeof(char * [res->params_count]));
    res->params[res->params_count - 1] = arg;
}

/*
    Converting a string to a specific type.
    And assign it to a pointer

    The "number" is similar to strncmp()'s "n"
 */
static void StringNumberToVariable_(char *source_str, int number, enum ArgpxVarType type, void **ptr)
{
    // prepare a separate string
    int str_len = strlen(source_str);
    int actual_len;
    size_t actual_size;
    // chose the smallest one
    if (number < str_len)
        actual_len = number;
    else
        actual_len = str_len;
    actual_size = actual_len * sizeof(char) + 1;
    // allocate a new string
    char *value_str = malloc(actual_size);
    memcpy(value_str, source_str, actual_size);
    value_str[actual_len] = '\0'; // actual_len is the last index of this string

    // remember to change the first level pointer, but not just change secondary one
    // if can, the value_str needs to free up
    // TODO implement other types
    switch (type) {
    case kArgpxVarTypeString:
        *ptr = value_str;
        break;
    case kArgpxVarTypeInt:
        break;
    case kArgpxVarTypeBool:
        break;
    case kArgpxVarTypeFloat:
        break;
    case kArgpxVarTypeDouble:
        break;
    }
}

enum ParamPartitionMode_ {
    kPartitionByArguments_,
    kPartitionByDelimiter_,
};

/*
    Use for ActionParamAny_()
 */
static enum ParamPartitionMode_ DetectParamPartitionMode_(struct UnifiedData_ data[static 1],
    struct ArgpxFlagGroup *group_ptr, struct ArgpxFlag conf_ptr[static 1], char *param, int param_range_len)
{
    enum ParamPartitionMode_ mode;
    char delim = group_ptr->delimiter;

    char *delim_ptr = memchr(param, delim, param_range_len);
    if (delim_ptr == NULL) {
        if ((group_ptr->attribute & ARGPX_GROUP_MANDATORY_DELIMITER) != 0)
            ArgpxExit_(data, kArgpxStatusRequiredDelimiter);
        mode = kPartitionByArguments_;
    } else {
        mode = kPartitionByDelimiter_;
    }

    return mode;
}

/*
    If param_start_ptr is NULL, then use the next argument string, which also respect the delimiter
 */
static void ActionParamAny_(
    struct UnifiedData_ data[static 1], struct ArgpxFlag conf_ptr[static 1], char *param_start_ptr)
{
    struct ArgpxFlagGroup *group_ptr = &data->groups[conf_ptr->group_idx];
    char delim = group_ptr->delimiter;
    int param_count;

    // for loop
    bool first_param = true;
    char *param_start;
    int param_len;
    char *next_delim_ptr;
    struct ArgpxParamUnit *unit_ptr;

    // init param_start
    switch (conf_ptr->action_type) {
    case kArgpxActionParamMulti:
        param_count = conf_ptr->action_load.param_multi.count;
        unit_ptr = conf_ptr->action_load.param_multi.units;
        break;
    case kArgpxActionParamSingle:
        param_count = 1;
        unit_ptr = &conf_ptr->action_load.param_single;
        break;
    default:
        ArgpxExit_(data, kArgpxStatusActionUnavailable);
        break;
    }

    if (param_start_ptr != NULL)
        param_start = param_start_ptr;
    else
        param_start = ShiftArguments_(data, 1);

    enum ParamPartitionMode_ partition_mode =
        DetectParamPartitionMode_(data, group_ptr, conf_ptr, param_start, strlen(param_start));

    for (int var_idx = 0; var_idx < param_count; var_idx++) {
        switch (partition_mode) {
        case kPartitionByArguments_:
            if (first_param == true)
                first_param = false;
            else
                param_start = ShiftArguments_(data, 1);

            param_len = strlen(param_start);
            break;
        case kPartitionByDelimiter_:
            if (first_param == true)
                first_param = false;
            else
                param_start = next_delim_ptr + 1;

            next_delim_ptr = strchr(param_start, delim);
            if (next_delim_ptr == NULL) {
                // if this is not the last parameter, the format is incorrect
                if (var_idx + 1 < param_count) {
                    ArgpxExit_(data, kArgpxStatusFlagParamDeficiency);
                }
                param_len = strlen(param_start);
            } else {
                param_len = next_delim_ptr - param_start;
            }
            break;
        }

        if (param_len == 0)
            ArgpxExit_(data, kArgpxStatusFlagParamDeficiency);

        // i know it's confusing...
        StringNumberToVariable_(
            param_start, param_len, unit_ptr[var_idx].type, unit_ptr[var_idx].ptr /* passing secondary pointer */);
    }
}

/*
    Invert the bool value of "toggle_ptr" in flag config
 */
static void ActionSetBool_(struct UnifiedData_ data[static 1], struct ArgpxFlag conf_ptr[static 1])
{
    struct ArgpxHidden_OutcomeSetBool *ptr = &conf_ptr->action_load.set_bool;
    *ptr->target_ptr = ptr->source;
}

/*
    Detect the group where the argument is located.
    A group index will be returned. Use GroupIndexToPointer_() convert it to a pointer.
    return:
      - INT_MIN: no group matched
      - other integer: give it to GroupIndexToPointer_() it will give back the group pointer
 */
static int DetectGroupIndex_(struct UnifiedData_ data[static 1])
{
    char *arg_ptr;
    struct ArgpxFlagGroup *g_ptr;
    int no_prefix_group_idx = -1;
    for (int g_idx = 0; g_idx < data->group_c; g_idx++) {
        arg_ptr = data->args[data->arg_idx];
        g_ptr = GroupIndexToPointer_(data, g_idx);

        if (g_ptr->prefix[0] == '\0')
            no_prefix_group_idx = g_idx;

        if (strncmp(arg_ptr, g_ptr->prefix, strlen(g_ptr->prefix)) == 0)
            return g_idx;
    }

    if (no_prefix_group_idx >= 0)
        return no_prefix_group_idx;

    return INT_MIN;
}

/*
    Should the assigner of this flag config is mandatory?
 */
static bool ShouldAssignerExist(
    struct UnifiedData_ data[static 1], struct ArgpxFlagGroup group_ptr[static 1], struct ArgpxFlag conf_ptr[static 1])
{
    switch (conf_ptr->action_type) {
    case kArgpxActionSetBool:
        return false;
        break;
    case kArgpxActionParamMulti:
    case kArgpxActionParamSingle:
        if ((group_ptr->attribute & ARGPX_GROUP_MANDATORY_ASSIGNER) != 0)
            return true;
        return false;
        break;
    default:
        ArgpxExit_(data, kArgpxStatusActionUnavailable);
        break;
    }

    return true;
}

/*
    Matching a name in all flag configs.
    It will find the conf with the highest match length in name_start.
    But if search_first == true, it will find the first matching one:

    e.g. Find "TestTest" and "Test" in "TestTest"
    false: -> "TestTest"
    true:  -> "Test"

    And flag may have assignment symbol, I think the caller should already known the names range.
    In this case, it make sense to get a max_name_len

    TODO this function parameter is so long
 */
static struct ArgpxFlag *MatchConfByName_(struct UnifiedData_ data[static 1], int g_idx, char *name_start,
    int max_name_len, bool search_first, bool with_excess)
{
    struct ArgpxFlag *conf_ptr;
    int conf_name_len;

    // we may need to find the longest match
    struct ArgpxFlag *final_conf_ptr = NULL;
    int final_name_len = 0;

    for (int conf_idx = 0; conf_idx < data->conf_c; conf_idx++) {
        conf_ptr = &data->confs[conf_idx];
        if (conf_ptr->group_idx != g_idx)
            continue;

        conf_name_len = strlen(conf_ptr->name);
        if (max_name_len < conf_name_len)
            continue;

        // matching name
        if (strncmp(name_start, conf_ptr->name, conf_name_len) != 0)
            continue;

        // update max length record
        if (final_name_len < conf_name_len) {
            final_name_len = conf_name_len;
            final_conf_ptr = conf_ptr;
        }

        if (search_first == true)
            break;
    }

    // if there is extra length, no matched
    if (max_name_len != final_name_len and with_excess != true)
        return NULL;

    return final_conf_ptr;
}

static void ParseArgumentIndependent_(
    struct UnifiedData_ data[static 1], int g_idx, struct ArgpxFlagGroup g_ptr[static 1])
{
    char *arg = data->args[data->arg_idx];
    char *assigner_ptr = strchr(arg, g_ptr->assigner); // if NULL no assigner
    char *name_start = arg + strlen(g_ptr->prefix);
    int name_len;
    if (assigner_ptr != NULL)
        name_len = assigner_ptr - name_start;
    else
        name_len = strlen(name_start);

    struct ArgpxFlag *conf_ptr = MatchConfByName_(data, g_idx, name_start, name_len, false, false);

    // some check
    if (conf_ptr == NULL)
        ArgpxExit_(data, kArgpxStatusUnknownFlag);
    if (assigner_ptr == NULL and ShouldAssignerExist(data, g_ptr, conf_ptr))
        ArgpxExit_(data, kArgpxStatusRequiredAssigner);

    // get flag parameters
    switch (conf_ptr->action_type) {
    case kArgpxActionSetBool:
        ActionSetBool_(data, conf_ptr);
        break;
    case kArgpxActionParamMulti:
    case kArgpxActionParamSingle:
        ActionParamAny_(data, conf_ptr, assigner_ptr + 1);
        break;
    default:
        ArgpxExit_(data, kArgpxStatusActionUnavailable);
        break;
    }
}

static void ParseArgumentGroupable_(
    struct UnifiedData_ data[static 1], int g_idx, struct ArgpxFlagGroup g_ptr[static 1])
{
    char *arg = data->args[data->arg_idx];
    char *assigner_ptr = strchr(arg, g_ptr->assigner); // if NULL no assigner
    int assigner_len;
    if (g_ptr->assigner != '\0')
        assigner_len = 1;
    else
        assigner_len = 0;

    // for loop
    char *name_start_ptr;
    int name_len;
    int prev_name_len;
    // how much length is available, including the current name in loop.
    // maybe it's a bad name
    int available_len;
    char *param_start_ptr;

    // init loop
    name_start_ptr = arg + strlen(g_ptr->prefix);
    prev_name_len = 0;
    if (assigner_ptr != NULL)
        available_len = assigner_ptr - name_start_ptr;
    else
        available_len = strlen(name_start_ptr);

    while (true) {
        name_start_ptr += prev_name_len;
        available_len -= prev_name_len;
        // if there are no chars to parse, processing ends
        if (available_len <= 0)
            break;

        struct ArgpxFlag *conf_ptr = MatchConfByName_(data, g_idx, name_start_ptr, available_len, true, true);
        if (conf_ptr == NULL)
            ArgpxExit_(data, kArgpxStatusUnknownFlag);
        if (assigner_ptr == NULL and ShouldAssignerExist(data, g_ptr, conf_ptr))
            ArgpxExit_(data, kArgpxStatusRequiredAssigner);
        name_len = strlen(conf_ptr->name);

        switch (conf_ptr->action_type) {
        case kArgpxActionParamMulti:
        case kArgpxActionParamSingle:
            // get parameter start pointer
            // the assigner is the most decisive
            // TODO bug here
            if (available_len != name_len) {
                param_start_ptr = name_start_ptr + name_len;
                if (assigner_ptr != NULL)
                    param_start_ptr += assigner_len;
            } else {
                param_start_ptr = NULL;
            }

            ActionParamAny_(data, conf_ptr, param_start_ptr);
            // there is no need to continue the loop after this action
            // C can't break that's while(true) on there, but return this function is also works
            return;
            break;
        case kArgpxActionSetBool:
            ActionSetBool_(data, conf_ptr);
            break;
        default:
            ArgpxExit_(data, kArgpxStatusActionUnavailable);
            break;
        }

        prev_name_len = name_len;
    }
}

/*
    If ErrorCallback is NULL then use exit(EXIT_FAILURE),
    if is a function then need accept a result structure, this should help

    TODO I don't like this parameter style
 */
struct ArgpxResult *ArgpxMain(int argc, int arg_base, char *argv[], struct ArgpxFlagGroup *groups, int group_count,
    struct ArgpxFlag *opts, int opt_count, void (*ErrorCallback)(struct ArgpxResult *))
{
    struct UnifiedData_ data = {
        .res = &(struct ArgpxResult){.status = kArgpxStatusSuccess, .argc = argc, .argv = argv},
        .ErrorCallback = ErrorCallback,
        .arg_c = argc,
        .args = argv,
        .arg_idx = arg_base,
        .groups = groups,
        .group_c = group_count,
        .conf_c = opt_count,
        .confs = opts,
    };

    for (; data.arg_idx < data.arg_c == true; data.arg_idx++) {
        // update index record
        data.res->current_argv_idx = data.arg_idx;
        data.res->current_argv_ptr = data.args[data.arg_idx];

        int g_idx = DetectGroupIndex_(&data);
        struct ArgpxFlagGroup *g_ptr = GroupIndexToPointer_(&data, g_idx);
        if (g_ptr == NULL) {
            AppendCommandParameter_(&data);
            continue;
        }

        if ((g_ptr->attribute & ARGPX_GROUP_FLAG_GROUPABLE) != 0)
            ParseArgumentGroupable_(&data, g_idx, g_ptr);
        else
            ParseArgumentIndependent_(&data, g_idx, g_ptr);
    }

    // TODO find a way to free those malloc()
    return data.res;
}
