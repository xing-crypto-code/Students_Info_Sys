# addStudent 函数流程图

对应实现：`Src/stu.c`

```mermaid
flowchart TD
    A([开始]) --> B[传入 Student 指针 stu]
    B --> C{学生信息是否合法?}
    C -- 否 --> D[返回 0：添加失败]
    C -- 是 --> E{studentCount >= MAX_STUDENT?}
    E -- 是 --> D
    E -- 否 --> F[遍历已有学生]
    F --> G{是否存在相同学号?}
    G -- 是 --> D
    G -- 否 --> H[students[studentCount] = *stu]
    H --> I[studentCount++]
    I --> J[返回 1：添加成功]
    D --> K([结束])
    J --> K
```

## 校验内容

1. `stu` 不能是空指针。
2. 学号必须大于 `0`。
3. 姓名不能为空。
4. 成绩必须在 `0` 到 `100` 之间。
5. 学生数量不能超过 `MAX_STUDENT`。
6. 学号不能与已有学生重复。
