# Лабораторная работа №1 — основы 3D-графики

**Шитов Никита Владиславович, М80-314БВ-24.** Номер в списке — 19:
`((19 - 1) % 12) + 1 = 7`. **Вариант 7 — тор.**

Реализовано основное задание на C++20 с Vulkan, GLFW и Dear ImGui: один тор, перспективная проекция, буфер глубины и осмотр с помощью камеры. Поверхность содержит 2048 вершин и 4096 треугольников. Цвет фиксирован; освещение помогает различать форму поверхности.

## Запуск и сборка

На этом компьютере зависимости настроены в `.local/`, приложение собирается в `build-debug/`:

```bash
./run.sh
```

Повторная сборка и тест геометрии:

```bash
./build.sh
```

Для доступа к компилятору шейдеров из bash/zsh:

```bash
source tools/activate.sh
glslc --version
```

В области сцены зажмите ЛКМ для вращения камеры, используйте колесо для приближения и отдаления. Кнопка «Сбросить камеру» возвращает исходный ракурс. `Esc` закрывает окно.

## Другой Linux

Нужны компилятор C++20, CMake 3.20+, Git, Vulkan headers/loader, glslc, заголовки X11 и драйвер Vulkan. Для Ubuntu/Debian:

```bash
sudo apt install build-essential cmake git libvulkan-dev glslc vulkan-validationlayers \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxfixes-dev libxrender-dev
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel 4
./build-debug/cg-lab-01
```

Для локальной настройки без прав администратора используйте `./tools/setup-local.sh`, затем `./build.sh`. Скрипт рассчитан на x86_64 и apt-репозитории Debian/Kali/Ubuntu; компилятор, CMake, Git, libX11-dev и Vulkan-драйвер должны уже быть установлены. GLFW использует X11; в Wayland-сеансе нужен XWayland.

CMake при первой конфигурации скачивает GLFW 3.4, vk-bootstrap v1.4.321, VMA v3.4.0 и ImGui v1.92.9 с GitHub. Шейдеры компилируются автоматически. Рабочая директория запуска произвольная: пути к ресурсам задаёт CMake.

## Windows

Установите Visual Studio 2022 с C++, CMake, Git и Vulkan SDK; проверьте `glslc --version`:

```powershell
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64
cmake --build build-windows --config Debug --parallel 4
.\build-windows\Debug\cg-lab-01.exe
```

Сборка и запуск проверены на Linux. Windows-конфигурация предусмотрена, но здесь не проверялась.

## Проверка и отчёт

```bash
ctest --test-dir build-debug --output-on-failure
./tools/verify.sh
```

Автоматическая проверка использует Xvfb, xauth, Python с Pillow и обязательные Vulkan Validation Layers с проверкой синхронизации. Проверяются геометрия, перспективная матрица, два ракурса камеры, изменение размера окна и наличие изображения тора. В виртуальном окне используется Mesa llvmpipe; сообщение о DRI3 относится к Xvfb.

Для отдельного снимка:

```bash
./run.sh --frames 60 --view side --capture report/screenshots/side.ppm
```

`--view default|side` выбирает начальный ракурс камеры; `--frames` задаёт число кадров до завершения. PPM копируется из Vulkan swapchain после отрисовки сцены и интерфейса.

Отчёт: `report/314_Шитов_Лаб1.pdf` и `.docx`. Исходное задание и шаблон находятся в `materials/`, скриншоты — в `report/screenshots/`, логи — в `report/checks/`. `python3 tools/make_report.py` формирует отчёт повторно (нужны python-docx и WeasyPrint).

## Исходники

- `source/application.cpp` — ресурсы Vulkan, интерфейс камеры и отрисовка одного тора.
- `source/scene.hpp` — геометрия тора, матрицы камеры и перспективы.
- `source/graphics_internal.cpp` — инфраструктура Vulkan, обработка resize и сохранение кадров.
- `shaders/` — GLSL-шейдеры.
- `tests/scene_tests.cpp` — проверка замкнутости поверхности, нормалей и матриц.

За основу взят [стартовый проект преподавателя](https://github.com/vladeemerr/vulkan-starter-app), коммит `7049a07b14f3f0deae7c9c0d3754bee88f876df0`. Лицензия сохранена в LICENSE и NOTICE. Python используется только для проверки скриншотов и создания отчёта; само приложение реализовано на C++.
