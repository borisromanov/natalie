#include "builtin.hpp"
#include "natalie.hpp"

namespace Natalie {

NatObject *Comparable_eqeq(NatEnv *env, NatObject *self, ssize_t argc, NatObject **args, NatBlock *block) {
    NAT_ASSERT_ARGC(1);
    NatObject *result = nat_send(env, self, "<=>", argc, args, NULL);
    if (NAT_TYPE(result) == NAT_VALUE_INTEGER && NAT_INT_VALUE(result) == 0) {
        return NAT_TRUE;
    } else {
        return NAT_FALSE;
    }
}

NatObject *Comparable_neq(NatEnv *env, NatObject *self, ssize_t argc, NatObject **args, NatBlock *block) {
    NAT_ASSERT_ARGC(1);
    NatObject *result = nat_send(env, self, "<=>", argc, args, NULL);
    if (NAT_TYPE(result) == NAT_VALUE_INTEGER && NAT_INT_VALUE(result) == 0) {
        return NAT_FALSE;
    } else {
        return NAT_TRUE;
    }
}

NatObject *Comparable_lt(NatEnv *env, NatObject *self, ssize_t argc, NatObject **args, NatBlock *block) {
    NAT_ASSERT_ARGC(1);
    NatObject *result = nat_send(env, self, "<=>", argc, args, NULL);
    if (NAT_TYPE(result) == NAT_VALUE_INTEGER && NAT_INT_VALUE(result) < 0) {
        return NAT_TRUE;
    } else {
        return NAT_FALSE;
    }
}

NatObject *Comparable_lte(NatEnv *env, NatObject *self, ssize_t argc, NatObject **args, NatBlock *block) {
    NAT_ASSERT_ARGC(1);
    NatObject *result = nat_send(env, self, "<=>", argc, args, NULL);
    if (NAT_TYPE(result) == NAT_VALUE_INTEGER && NAT_INT_VALUE(result) <= 0) {
        return NAT_TRUE;
    } else {
        return NAT_FALSE;
    }
}

NatObject *Comparable_gt(NatEnv *env, NatObject *self, ssize_t argc, NatObject **args, NatBlock *block) {
    NAT_ASSERT_ARGC(1);
    NatObject *result = nat_send(env, self, "<=>", argc, args, NULL);
    if (NAT_TYPE(result) == NAT_VALUE_INTEGER && NAT_INT_VALUE(result) > 0) {
        return NAT_TRUE;
    } else {
        return NAT_FALSE;
    }
}

NatObject *Comparable_gte(NatEnv *env, NatObject *self, ssize_t argc, NatObject **args, NatBlock *block) {
    NAT_ASSERT_ARGC(1);
    NatObject *result = nat_send(env, self, "<=>", argc, args, NULL);
    if (NAT_TYPE(result) == NAT_VALUE_INTEGER && NAT_INT_VALUE(result) >= 0) {
        return NAT_TRUE;
    } else {
        return NAT_FALSE;
    }
}

}
