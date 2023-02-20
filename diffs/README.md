git diff result, shows what is modified from our repos to the original Ivan-Velickovic's repos.

These diff files can then be applied back to the original repos into our repos.


Q: How to apply these patches?

A: Example: sel4cp

```sh
git clone https://github.com/Ivan-Velickovic/sel4cp --branch dev
cd sel4cp
patch -p1 < ../diffs/sel4cp.diff
```

