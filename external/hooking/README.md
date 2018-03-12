Hooking.Patterns
----------------
Sample:

```cpp
#include "stdafx.h"
#include <Windows.h>
#include "Hooking.Patterns.h"

int main()
{
    auto pattern = hook::pattern("5? ?8 ?? ? 20    70 72 6F 67 72");
    if (pattern.size() > 0)
    {
        auto text = pattern.get(0).get<char>(0);
        MessageBoxA(0, text, text, 0);
    }
    return 0;
}
```

Result:

![MessageBox](http://i.imgur.com/Tuijf2I.png)
