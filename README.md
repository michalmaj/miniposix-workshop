# MiniPOSIX – programowanie, testowanie i analiza w C++23

Repozytorium startowe zadania semestralnego dla studentów mechatroniki. Rozwiniesz niewielką bibliotekę C++23 korzystającą z mechanizmów POSIX, sprawdzisz ją testami i przeanalizujesz pomiary transferu danych. Dokładny kontrakt i wymagane ćwiczenia znajdują się w [ASSIGNMENT.md](ASSIGNMENT.md).

Nie musisz znać wcześniej POSIX-a. **Deskryptor pliku** to liczba identyfikująca otwarty zasób systemu. **Potok (pipe)** łączy koniec do zapisu z końcem do odczytu. Główne funkcje biblioteki są w starterze celowo niedokończone.

## Wymagania dotyczące środowiska

Do wykonania zadania wymagany jest **Linux lub macOS**. Osoby korzystające z Windows muszą przygotować maszynę wirtualną z Linuxem, np. Ubuntu 24.04 LTS w Oracle VirtualBox. Zadanie wykorzystuje mechanizmy POSIX i nie jest przeznaczone do uruchamiania bezpośrednio w Windows. Repozytorium sklonuj w odpowiednim systemie; tam wykonaj kompilację, testy, analizę, lokalny commit oraz `git archive`. W przypadku Windows wszystkie te kroki wykonaj wewnątrz maszyny wirtualnej.

- LLVM/Clang 22 z obsługą C++23. Presety CMake wskazują nazwy `clang-22` i `clang++-22`, dostępne na Linuxie po instalacji odpowiednich pakietów. Jeśli ich brakuje na Ubuntu, skorzystaj z [instrukcji pakietów LLVM](https://apt.llvm.org/) właściwej dla Twojej wersji systemu. Na macOS użyj konfiguracji opisanej niżej.
- CMake 3.28 lub nowszy, Ninja, Git, `unzip` oraz pakiet deweloperski GoogleTest. Na Ubuntu zależność testów można zainstalować poleceniem `sudo apt install libgtest-dev`.
- [uv](https://docs.astral.sh/uv/getting-started/installation/) i Python 3.12 lub nowszy do analizy CSV. `uv` tworzy lokalne środowisko i może pobrać odpowiednią wersję Pythona; szczegóły są w [analysis/README.md](analysis/README.md).
- Dostęp do dowolnego narzędzia AI na potrzeby obowiązkowego ćwiczenia weryfikacyjnego. Przy implementacji użycie AI jest opcjonalne. Możesz użyć np. [OpenCode](https://opencode.ai/docs/); zadanie nie wymaga konkretnego płatnego modelu.

Jeśli nie masz dostępu do narzędzia AI potrzebnego do ćwiczenia weryfikacyjnego, zgłoś to prowadzącemu przed rozpoczęciem tej części zadania.

Jeśli brakuje `uv`, zainstaluj je według [oficjalnej instrukcji](https://docs.astral.sh/uv/getting-started/installation/), na przykład poleceniem `curl -LsSf https://astral.sh/uv/install.sh | sh`. Otwórz nowy terminal i sprawdź `uv --version`. Pakiety do analizy zostaną zainstalowane w lokalnym `analysis/.venv`.

Na macOS możesz zainstalować narzędzia C++ przez [Homebrew](https://brew.sh/):

```bash
brew install llvm@22 cmake ninja googletest
"$(brew --prefix llvm@22)/bin/clang++" --version
```

Homebrew instaluje LLVM 22 poza domyślną ścieżką kompilatorów. Dlatego na macOS w poleceniach konfiguracji CMake poniżej podajemy ścieżki przez `brew --prefix llvm@22`; wskazujemy również lokalizację GoogleTest. Jeżeli używasz CLion, ustaw jego toolchain i opcje CMake na te same ścieżki. Wymagane są też Apple Command Line Tools (w razie potrzeby: `xcode-select --install`).

Przed rozpoczęciem pracy sprawdź wersje narzędzi oraz dostępność `unzip`:

```bash
cmake --version
ninja --version
git --version
uv --version
command -v unzip
```

Na Linuxie sprawdź także `clang-22 --version` i `clang++-22 --version`.

## Pobranie projektu

W terminalu Linux lub macOS (na Windows: wewnątrz maszyny wirtualnej) sklonuj repozytorium i przejdź do jego katalogu:

```bash
git clone https://github.com/michalmaj/miniposix-workshop.git
cd miniposix-workshop
```

Przeczytaj [ASSIGNMENT.md](ASSIGNMENT.md) i publiczne nagłówki w `include/miniposix/`. Pracuj na lokalnej kopii. Do oddania zadania nie potrzebujesz forka, wysyłania zmian na GitHuba ani pull requesta.

## Budowanie i testy

Z głównego katalogu repozytorium skonfiguruj wariant Debug. Na Linuxie:

```bash
cmake --preset clang22-debug
```

Na macOS:

```bash
cmake --preset clang22-debug \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm@22)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm@22)/bin/clang++" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix googletest)"
```

Następnie na obu systemach:

```bash
cmake --build --preset clang22-debug --parallel
ctest --preset clang22-debug-smoke --no-tests=error
ctest --preset clang22-debug-functional --no-tests=error
```

W niezmienionym starterze test `smoke` powinien przejść, a część testów funkcjonalnych powinna się nie powieść: implementacja jest Twoim zadaniem. Po poprawkach uruchamiaj testy ponownie. Wszystkie zarejestrowane testy możesz uruchomić poleceniem `ctest --test-dir build/clang22-debug --output-on-failure`.

Do diagnozowania błędów możesz użyć kompilacji Debug, debuggera, a opcjonalnie ASan/UBSan; na Linuxie przydatny może być też `strace`. Sama udana kompilacja nie potwierdza poprawności zarządzania deskryptorami; sprawdzaj zachowanie opisane w nagłówkach i [ASSIGNMENT.md](ASSIGNMENT.md).

## Benchmark i analiza CSV

Po przejściu testów biblioteki skonfiguruj wariant zoptymalizowany. Na Linuxie:

```bash
cmake --preset clang22-perf
```

Na macOS:

```bash
cmake --preset clang22-perf \
  -DCMAKE_C_COMPILER="$(brew --prefix llvm@22)/bin/clang" \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm@22)/bin/clang++" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix googletest)"
```

Następnie na obu systemach zbuduj benchmark i wykonaj pomiary:

```bash
cmake --build --preset clang22-perf --parallel
mkdir -p bench-results
./build/clang22-perf/bench_pipe --output bench-results/pipe.csv
uv sync --project analysis --locked
uv run --project analysis --locked python analysis/analyze.py bench-results/pipe.csv --plot bench-results/throughput.png
```

Domyślnie benchmark przesyła 8 MiB trzykrotnie dla każdego z buforów 512, 4096 i 65536 bajtów. Parametry można zmienić przez `--bytes N --repeats N --buffer-sizes 512,4096,65536`. Program sprawdza przesłane bajty i **dopisuje** wiersze do CSV, więc osobny pomiar zapisuj pod nową nazwą. Analizuj surowe dane i wykres. Pomiar obejmuje cały scenariusz transferu, a nie sam koszt `UniqueFd`; używaj presetu `clang22-perf`, a nie Debug.

Jeśli nie ukończysz biblioteki, do części analitycznej możesz użyć CSV udostępnionego przez prowadzącego. Zapisz jego pochodzenie i nie przedstawiaj takich pomiarów jako własnych. Instrukcja instalacji `uv` i uruchamiania analizy znajduje się także w [analysis/README.md](analysis/README.md).

## Pliki do oddania

Utwórz katalog `submission/` i zapisz w nim materiały do oceny:

```bash
mkdir -p submission
```

- `submission/raport.md` – wyniki testów, opis prób i poprawek, interpretacja pomiarów, ograniczenia wniosków oraz odpowiedzi wymagane w [ASSIGNMENT.md](ASSIGNMENT.md).
- `submission/ai-evidence.json` – dowód weryfikacji twierdzenia AI według pól wskazanych w zadaniu.
- `submission/pipe.csv` – własny wynik benchmarku albo CSV od prowadzącego; w raporcie oznacz źródło danych.
- `submission/throughput.png` – wykres, jeśli udało się uruchomić skrypt analizy.

Katalog `bench-results/` jest ignorowany przez Git. Jeśli benchmark i skrypt zapisały tam wyniki, skopiuj je przed commitem:

```bash
cp bench-results/pipe.csv submission/pipe.csv
cp bench-results/throughput.png submission/throughput.png
```

Jeżeli analizujesz CSV od prowadzącego, skopiuj **ten** plik do `submission/pipe.csv` i zapisz jego pochodzenie w raporcie. Nie dodawaj do commita katalogów budowania, środowiska `analysis/.venv`, danych uwierzytelniających ani plików tymczasowych.

## Lokalny commit i archiwum do Moodle

Po zakończeniu pracy sprawdź zmiany, dodaj rozwiązanie i utwórz lokalny commit:

```bash
git status
git diff
git add include src tests submission
git commit -m "Complete MiniPOSIX assignment"
git status
```

Jeżeli zmieniłeś inne potrzebne pliki, dodaj je świadomie do commita. Po commicie katalog roboczy powinien być czysty. Archiwum utwórz **wyłącznie poleceniem `git archive`**, z głównego katalogu repozytorium:

```bash
git archive --format=zip --output=../miniposix_Nazwisko_Imie.zip HEAD
unzip -l ../miniposix_Nazwisko_Imie.zip
```

Zastąp `Nazwisko_Imie` własnymi danymi w obu poleceniach. `git archive` umieszcza w ZIP tylko pliki zapisane w commicie `HEAD`. Sprawdź, czy lista zawiera zmienione źródła, testy i wymagane pliki z `submission/`. Prześlij **jeden** plik ZIP do Moodle. Nie przesyłaj katalogu `.git`, plików wykonywalnych ani archiwum pobranego bezpośrednio ze strony GitHub.

Prowadzący może poprosić o wyjaśnienie kodu, wyników testów, analizy CSV i decyzji podjętych po weryfikacji AI.
