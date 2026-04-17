# sensor

这个仓库用于存放多个独立传感器相关项目。根目录下每个一级文件夹都视为一个独立项目。

## 项目列表

运行 `python sync_sensor_to_github.py` 时会自动刷新下表。

<!-- PROJECT_TABLE_START -->
| 项目名 | 简介 | 路径 |
|---|---|---|
| Mygame_4_15 | 这是一个基于 Python + Pygame 的离线版小恐龙游戏，支持 Arduino 传感器输入： | `Mygame_4_15` |
<!-- PROJECT_TABLE_END -->

## 自动同步到 GitHub

在 `sensor` 根目录执行：

```bash
python sync_sensor_to_github.py
```

可选参数示例：

```bash
python sync_sensor_to_github.py --remote origin --branch main
python sync_sensor_to_github.py --repo-url git@github.com:yourname/your-repo.git
```

说明：

- 脚本会先自动更新本 README 的项目表格。
- 然后执行 `git add -A`、`git commit`、`git push`。
- 如果没有代码变化，会提示无需提交。
