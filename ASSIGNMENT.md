# MiniPOSIX — implementacja, testowanie i analiza w C++23

Rozwijasz niewielką bibliotekę korzystającą z mechanizmów POSIX w systemie Linux lub macOS. Repozytorium jest starterem: główne funkcje biblioteki są celowo niedokończone. Zadanie wykonujesz samodzielnie w lokalnej kopii repozytorium. Wymagania środowiskowe i sposób przygotowania archiwum do Moodle opisuje [README.md](README.md).

Możesz używać AI przy implementacji, ale nie jest to wymagane. **Obowiązkowa jest część weryfikacyjna:** porównasz dwie odpowiedzi AI na to samo pytanie o zachowanie POSIX i sprawdzisz wybrane twierdzenie dokumentacją oraz testem. Możesz użyć dowolnego dostępnego narzędzia AI; OpenCode jest jedną z możliwości. Każdą przyjętą sugestię musisz rozumieć i umieć wyjaśnić.

## 1. Przygotowanie

Przeczytaj [README.md](README.md), sprawdź narzędzia, skonfiguruj preset `clang22-debug` i uruchom osobno test infrastruktury oraz testy funkcjonalne. Na niezmienionym starterze test `smoke` powinien przejść, a część testów funkcjonalnych powinna zakończyć się niepowodzeniem. Zanotuj wynik początkowy w `submission/raport.md`.

Warto przed kodowaniem sprawdzić znaczenie pojęć: deskryptor pliku, `open`, `close`, przenośne wywołanie `pipe`, `read`, `write`, `errno`, RAII, konstruktor i operator przenoszący oraz `std::system_error`.

## 2. Implementacja i testy

Zaimplementuj kontrakt opisany w publicznych nagłówkach:

- `miniposix::UniqueFd` posiada co najwyżej jeden deskryptor. Domyślny stan jest nieprawidłowy (`-1`). Destruktor zamyka posiadany deskryptor. `get()` odczytuje jego wartość, `release()` oddaje własność bez zamykania, a `reset()` zastępuje deskryptor lub zwalnia zasób. Przeniesienie przekazuje własność i unieważnia źródło. Kopiowanie jest zabronione.
- `miniposix::Pipe::create()` tworzy koniec do odczytu i koniec do zapisu albo zgłasza błąd systemu jako `std::system_error`. `read_end()` i `write_end()` udostępniają referencje do obiektów `UniqueFd`; surowy deskryptor do operacji POSIX uzyskasz przez `.get()`. Sygnatury i szczegóły błędów są w nagłówkach.
- Poprawność obejmuje czas życia zasobów. Test sprawdzający tylko wartość `get()` nie wykryje każdego wycieku. Sprawdź także, czy stary deskryptor rzeczywiście został zamknięty po zmianie własności. Możesz dodać własny, ukierunkowany test.

Zbuduj projekt i uruchom testy po każdej istotnej zmianie. Przy niepowodzeniu sprawdź faktyczne zachowanie deskryptorów, popraw kod lub test i zapisz w `submission/raport.md`, co było przyczyną poprawki. Jeżeli używasz AI do pisania kodu, czytaj proponowane zmiany i weryfikuj istotne twierdzenia w dokumentacji C++/POSIX.

Możesz dodatkowo zaprojektować pomocniczą operację I/O. Musi ona odróżniać koniec danych (EOF), przerwanie wywołania (`EINTR`), błąd i częściowy transfer. `fork`, `exec`, obsługa sygnałów, `poll` i pętla zdarzeń nie należą do obowiązkowej implementacji.

W raporcie zapisz co najmniej krótką historię prób:

| Próba | Nieudany test lub obserwacja | Zmiana i jej uzasadnienie | Wynik po zmianie |
| --- | --- | --- | --- |
| 1 | | | |

## 3. Benchmark i analiza danych

Po przejściu testów funkcjonalnych uruchom benchmark `bench_pipe` w presecie `clang22-perf` i przeanalizuj CSV za pomocą skryptu w `analysis/`, zgodnie z [README.md](README.md). Benchmark sprawdza przesyłane bajty, obsługuje częściowe operacje I/O i zapisuje powtórzenia bez progu czasu decydującego o poprawności.

W `submission/raport.md` odpowiedz:

1. Który rozmiar bufora miał najwyższą medianę przepustowości? Czy kolejność wyników była stabilna między powtórzeniami?
2. Jakie czynności obejmuje pomiar? Dlaczego nie mierzy samego narzutu `UniqueFd`?
3. Które obserwacje mogą się zmienić na innym komputerze lub przy innym obciążeniu systemu? Jeśli pracujesz na Linuxie i użyłeś `perf stat`, co dodały jego liczniki?
4. Jak zmieniłbyś eksperyment, zanim uznałbyś jeden wariant za ogólnie szybszy?

Jeżeli nie ukończysz biblioteki na czas, poproś prowadzącego o zapasowy CSV i przeprowadź analizę tych danych. W raporcie podaj pochodzenie danych i wyraźnie zaznacz, które pomiary są Twoje. Nie wymyślaj wyników. Sugestie AI dotyczące interpretacji porównuj z surowym CSV, powtórzeniami i warunkami pomiaru.

Benchmark zapisuje dane domyślnie w ignorowanym przez Git `bench-results/`. Plik CSV potrzebny do oddania skopiuj do `submission/pipe.csv`. Wykres z analizy zapisz lub skopiuj do `submission/throughput.png`.

## 4. Weryfikacja odpowiedzi AI

Wybierz jedno konkretne pytanie o zachowanie POSIX istotne dla biblioteki, na przykład o własność deskryptora, EOF albo częściowe `read`/`write`. Zadaj **to samo pytanie** w dwóch oddzielnych rozmowach z AI lub dwóm dostępnym modelom. Zachowaj obie odpowiedzi. Jeśli narzędzie AI jest niedostępne, zgłoś to prowadzącemu, aby uzyskać odpowiedzi do oceny.

1. Porównaj sprawdzalne twierdzenia obu odpowiedzi. Zgodność odpowiedzi nie jest dowodem poprawności.
2. Wybierz jedno ważne twierdzenie. Znajdź odpowiednią dokumentację POSIX lub używanego systemu i zapisz jej adres oraz to, co mówi o twierdzeniu.
3. Uruchom mały test i zapisz polecenie oraz zaobserwowany wynik. Gdy Twoja biblioteka jest niedokończona, test może być samodzielnym programem. Wyjaśnij, jakie warunki sprawdza test i co nadal wymaga reguły z dokumentacji.
4. Zapisz w `submission/ai-evidence.json` rekord z polami `claim`, `source_url`, `test_command`, `observed` i `verdict`. Samodzielnie ustaw `verdict` na `accept`, `revise` albo `reject` po ocenie obu odpowiedzi, źródła i testu. Zaznacz pozostałą niepewność.
5. W `submission/raport.md` umieść obie odpowiedzi lub ich istotne fragmenty oraz narysuj prosty workflow (np. w Mermaid lub jako schemat tekstowy): pytanie → odpowiedzi AI → dokumentacja → test → **decyzja człowieka** → przyjęcie, poprawienie albo odrzucenie sugestii. Decyzja należy do Ciebie przed zapisaniem zaakceptowanej sugestii w commicie.

## Zadania dodatkowe

- Dodaj test przenoszącego przypisania `Pipe`. Przed przypisaniem obiekt docelowy powinien mieć własne deskryptory. Sprawdź wywołaniem POSIX, że stare deskryptory zostały zamknięte, i wyjaśnij, dlaczego same wartości przechowywane w obiektach nie wystarczają do wykrycia wycieku.
- Uruchom benchmark ponownie z tymi samymi parametrami transferu i co najmniej 10 powtórzeniami, zapisując wyniki do nowej ścieżki CSV. Porównaj medianę i rozrzut ze swoim pierwszym pomiarem na tym samym komputerze. Wskaż zmienne zapisane w CSV oraz brakujące warunki, takie jak procesor, obciążenie, system operacyjny i kompilator. Wyjaśnij ich wpływ na wnioski.

## 5. Pliki do oddania

W lokalnym repozytorium przygotuj:

- `submission/raport.md` — początkowe i końcowe wyniki testów, historia prób, odpowiedzi na pytania o benchmark, pochodzenie danych, porównanie dwóch odpowiedzi AI, schemat weryfikacji i ograniczenia wniosków. Dodaj krótki szkic możliwego rozszerzenia, np. o proces potomny lub interfejs zdarzeniowy, ze wskazaniem nowych przypadków własności i błędów; implementacja rozszerzenia nie jest wymagana.
- `submission/ai-evidence.json` — rekord dowodowy z części 4.
- `submission/pipe.csv` — własne dane benchmarkowe albo CSV otrzymany od prowadzącego, z podanym w raporcie pochodzeniem.
- `submission/throughput.png` — wykres, jeśli udało Ci się uruchomić skrypt analizy; brak wykresu wyjaśnij w raporcie.

Do commita dodaj także zmienione pliki źródłowe i własne testy. Wszystkie materiały stanowiące rozwiązanie muszą znaleźć się w lokalnym commicie przed wykonaniem `git archive`. Archiwum ZIP do Moodle przygotuj według [README.md](README.md). Prowadzący może poprosić o wyjaśnienie kodu, testów i analizy.
