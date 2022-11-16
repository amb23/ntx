# ntx
Tools for compiling ntx files

ntx is a shorthand note taking format. It is built off indents, e.g.
```
\corrollary
    e^{i\pi} + 1 = 0
\proof
    We have already seen that for real \theta
    \eq
        \cos(\theta) + i\sin(\theta) = e^{i\theta}
    ...
```
The compiler will add inline math tags as well as properly building tags to generate a tex file that can then be included in a larger tex project
