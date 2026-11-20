#include "commands.h"
#include "contracts.h"

int main(void)
{
    const Contracts contracts = {prime_count, e};
    return run_commands(&contracts, 0);
}
