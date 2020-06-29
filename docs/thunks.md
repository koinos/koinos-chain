### Thunks

We need to allow VM functions to call into native code. To achieve this, we use thunks.

To understand how thunks work, first consider a hypothetical simpler solution: Simply let VM code call directly to an address in native code. There are several obvious problems with this:

- If the pointer doesn't point to the legitimate entry point to the beginning of a native code function that expects to be called from inside the VM, you might have problems like crashing, corruption, or remote code execution security.
- Functions won't always be at the same address due to ASLR, or simply changes in the size of various parts of the codebetween otherwise semantically equivalent builds.

It's clear how to fix these problems:

- Implement a whitelist. Only specifically registered addresses can be called.
- Implement a translation layer. Don't give out literal addresses, instead give out a fake address. Then use a lookup table to translate fake address to a real native address.

The lookup table can be recycled to implement the whitelist; simply forbid any call that's not in the lookup table.

The lookup table is implemented in a class called `thunk_dispatcher`. The LUT is created at program initialization in `thunk_dispatcher.cpp`.

- An entry in the thunk lookup table is permanent.
- If you create a new native function, it gets a new thunk ID.

### Sysytem Calls

Now we have a separate problem. What is our plan for an in-band upgrade of functionality that's initially implemented in native code?

Another layer of table-based call dispatching, `sysem_call`, is implemented to solve this problem.

User smart contracts cannot call thunks directly. Instead, smart contracts call thunks by specifying a key which points into the system call translation table. The keys of the xcall translation table are integers, values are of type `xcall_target` which is an `std::variant` which can contain a `thunk_id_type` or a `contract_id_type`.

A system call can be upgraded by writing a different `system_call_target` to the state.

### Native-to-native code updates

Here's how to make (semantically inequivalent, potentially consensus-breaking) updates to a native xcall handler as native code:

- Treat the updated function *as if you're creating a new native function*.
- Register the new function under a brand-new thunk ID.
- Keep the old function registered under the old thunk ID.
- When business logic determines switchover should occur, write the thunk ID into the xcall.

WASM code can call thunks directly via the `invoke_thunk` syscall. Code which calls a thunk directly can only be upgraded by replacing it with different code. Therefore, only system code should be allowed to call `invoke_thunk`.

### Calling thunk functions

Let's consider the upgrade path for a natively implemented system call handler. Let's use `prints()` as an example, but really it applies to any xcall.

For a more concrete example, let's say `prints()` is an xcall that (for consensus purposes) is a no-op at launch (prints may put its output somewhere that may be seen by a debugging tool, or literally log it to the console if you run koinosd with full verbosity, but that's irrelevant to consensus). But then we want to have a hardfork to hash the `prints()` output and put the hash somewhere (perhaps in passive data).

Re-writing the new version of `prints()` is straightforward. Let's instead focus on other natively implemented system calls that call `prints()`.

When declaring thunks, we declare both the thunk implementation and a system call wrapper such that `invoke_thunk` and `invoke_system_call` semantics are both exposed natively.

### Creating a thunk

There are few extra steps required to create a new thunk beyond declaring and defining a function body.

Add a declaration in `thunks.hpp`. Thunk declarations look similar to normal function declaration, but using the `THUNK_DECLARE` macro.

An example is: `THUNK_DECLARE( bool, verify_block_header, const crypto::recoverable_signature& sig, const crypto::multihash_type& digest );`

For void arguments, there is a special `THUNK_DECLARE_VOID` macro, `THUNK_DECLARE_VOID( variable_blob, get_contract_args );`.

Thunk definitions are done in `thunks.cpp`. Definitions also look similar to function definitions but require arguments be declared in a bubble list style syntax required by the macro to parse out arguments and types properly.

The example for these declarations are:

`THUNK_DEFINE( bool, verify_block_header, ((const crypto::recoverable_signature&) sig, (const crypto::multihash_type&) digest) )`

`THUNK_DEFINE_VOID( variable_blob, get_contract_args )`

Three other additions are needed. There a bubble lists in `thunks.cpp` and `system_calls.cpp`, `REGISTER_THUNKS` and `DEFAULT_SYSTEM_CALLS`. A thunk id needs to be declared in `thunks.hpp` in the `thunk_ids` macro.

`REGISTER_THUNKS` registers all thunks to their thunk id.

`DEFAULT_SYSTEM_CALLS` defines which thunks are available initially via `invoke_system_calls`. If thunks are added later, after the genesis of Koinos main net, those thunks should not be `DEFAULT_SYSTEM_CALLS`.
