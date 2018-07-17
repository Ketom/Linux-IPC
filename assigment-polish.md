# SO - zadanie A7

I. Opracować zestaw programów typu producent - konsument realizujących przy wykorzystaniu mechanizmu **łączy nazwanych** (**kolejek FIFO**), następujący schemat komunikacji międzyprocesowej:

*Proces 1*: czyta dane (pojedyncze wiersze) ze standardowego strumienia wejściowego i przekazuje je w niezmienionej formie do *procesu 2*.

*Proces 2*: pobiera dane przesłane przez *proces 1*. Oblicza ilość znaków w każdej linii i wyznaczoną liczbę przekazuje do *procesu 3*. 

*Proces 3*: pobiera dane wyprodukowane przez *proces 2* i umieszcza je w standardowym strumieniu wyjściowym. Każda odebrana jednostka danych powinna zostać wyprowadzona w osobnym wierszu.

II. Należy zaproponować i zaimplementować mechanizm informowania się procesów o swoim stanie. Należy wykorzystać do tego dostępny mechanizm **sygnałów** i **kolejek komunikatów**. Wszystkie trzy procesy powinny być powoływane automatycznie z jednego procesu inicjującego.

Należy zaimplementować mechanizm asynchronicznego przekazywania informacji pomiędzy operatorem a procesami oraz pomiędzy procesami. Wykorzystać do tego dostępny mechanizm **sygnałów** i **kolejek komunikatów**.

Operator może wysłać do dowolnego procesu sygnał zakończenia działania (S1), sygnał wstrzymania działania (S2) i sygnał wznowienia działania (S3). Sygnał S2 powoduje wstrzymanie synchronicznej wymiany danych pomiędzy procesami. Sygnał S3 powoduje wznowienie tej wymiany. Sygnał S1 powoduje zakończenie działania oraz zwolnienie wszelkich wykorzystywanych przez procesy zasobów. Każdy z sygnałów przekazywany jest przez operatora tylko do jednego, dowolnego procesu. O tym, do którego procesu wysłać sygnał, decyduje operator, a nie programista.

Każdy z sygnałów operator może wysłać do innego procesu. Mimo, że operator kieruje sygnał do jednego procesu, to pożądane przez operatora działanie musi zostać zrealizowane przez wszystkie trzy procesy. W związku z tym, proces odbierający sygnał od operatora musi powiadomić o przyjętym żądaniu pozostałe dwa procesy. Powinien wobec tego wysłać do nich sygnał (S4) oraz przekazać informację o tym jakiego działania wymaga operator, przekazując im stosowny komunikat (lub komunikaty) poprzez mechanizm **sygnałów** i **kolejek komunikatów**. Procesy odbierające sygnał S4, powinny odczytać skierowany do nich komunikat (lub komunikaty) w procedurze odbierania sygnału S4. Wszystkie trzy procesy powinny zareagować zgodnie z żądaniem operatora.

Sygnały oznaczone w opisie zadania symbolami S1, S2, S3, S4 należy wybrać samodzielnie spośród dostępnych w systemie (np. SIGUSR1, SIGUSR2, SIGINT, SIGCONT).
