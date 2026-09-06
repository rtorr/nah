#include "nah/nah_exec.h"

#include <doctest/doctest.h>

TEST_CASE("execution contract validation") {
    nah::core::LaunchContract contract;

    CHECK(nah::exec::validate_contract(contract) == "execution binary is empty");

    contract.execution.binary = "/bin/example";
    CHECK(nah::exec::validate_contract(contract).empty());

    contract.environment["BAD=KEY"] = "value";
    CHECK(nah::exec::validate_contract(contract) == "invalid environment key");

    contract.environment.clear();
    contract.execution.arguments.emplace_back("before\0after", 12);
    CHECK(nah::exec::validate_contract(contract) == "execution argument contains NUL");
}
