# 🧾 Prompt Syntax Reference (Inja Template Syntax)

## 🔧 Expressions (`{{ ... }}`)

Used to render variables and evaluate expressions:

- `{{ my_variable }}`
- `{{ decnpc(npc.UUID).name }}`
- `{{ array.1 }}`
- `{{ variable + 1 }}`
- `{{ my_mods_custom_decorator(npc.UUID) }}`

---

## 💬 Comments (`{# ... #}`)

- `{# This is a comment #}`

---

## 🧠 Control Flow: Statements (`{% ... %}`)

### 🔁 Loops

```
{% for item in get_nearby_npc_list(npc.UUID) %}
  {{ item }}
{% endfor %}
```

```
{% for key, value in object %}
  {{ key }}: {{ value }}
{% endfor %}
```

**Loop variables:**

- `loop.index` – 0-based index
- `loop.index1` – 1-based index
- `loop.is_first` – true on first iteration
- `loop.is_last` – true on last iteration
- `loop.parent.index` – access outer loop index

---

### 🔀 Conditionals

```
{% if condition %}
  ...
{% else if other_condition %}
  ...
{% else %}
  ...
{% endif %}
```

Supported:
- Logical: `and`, `or`, `not`
- Comparisons: `<`, `>`, `==`, `!=`, `>=`, `<=`
- Membership: `value in list`
- Grouping: `( ... )`

---

### 🧱 Assignments

```
{% set variable = value %}
{% set object.key = value %}
```

Assigns a value for use in the current context (not persistent and not accessible from other .prompt files).

---

### 📎 Includes

```
{% include "template_name" %}
```

Includes other templates by name or filename.

---

## 🧬 Functions

Used inside expressions: `{{ function(...) }}`

| Function | Description |
|---------|-------------|
| `upper(str)` / `lower(str)` | Convert case |
| `range(n)` | 0 to n-1 |
| `at(arr, i)` | Index into array |
| `length(arr)` | Count elements |
| `first(arr)` / `last(arr)` | First/last element |
| `sort(arr)` | Sort values |
| `join(arr, sep)` | Join with separator |
| `round(num, digits)` | Round |
| `odd(num)` / `even(num)` | Check parity |
| `divisibleBy(a, b)` | Check divisibility |
| `max(arr)` / `min(arr)` | Extremes |
| `int("2")`, `float("3.1")` | Convert types |
| `default(val, fallback)` | Use fallback if val is undefined |
| `exists("key")` / `existsIn(obj, "key")` | Key presence |
| `isString(val)` / `isArray(val)` etc. | Type checks (also: isBoolean, isFloat, isInteger, isNumber, isObject) |

---

## 🧬 Template Inheritance

```
{% extends "base.html" %}

{% block title %}
  Page Title
{% endblock %}

{% block content %}
  Hello!
{% endblock %}
```

- `extends` must be first in file.
- Use `{{ super() }}` inside blocks to render parent content.
- Use `super(n)` to skip `n` inheritance levels.

---

## 🧼 Whitespace Control

### Global config:
- `trim_blocks = true` – Removes newline after statement block
- `lstrip_blocks = true` – Strips whitespace before block

### Manual trim with `-`:

```
Hello       {{- name -}}     !
{% if x -%}
  Trimmed
{%- endif %}
```

Removes whitespace before/after block and newlines.

---

## 💬 Comments

```
{# This is a comment #}
```

Ignored in rendered output.

---
