# Web ChatGPT та інші ШІ-сервіси
27.10.2026, Europe/Kyiv. Це гайд інтеграції; нові сервіси/ключі/підключення цією документаційною операцією не створювалися.

## Перевірений транспорт і межі
У цій сесії прямі виклики Desktop Commander виконали read-only інвентаризацію всіх трьох хостів. Доступні також вузькі CAD/CAE та EDA/FPGA MCP tools. Їхня наявність не надає довільного shell або PlutoSky deployment API. Раніше EDA registry не мав потрібного зареєстрованого workflow; його стан треба перечитати перед використанням.

27.10: tunnel-client не знайдений у PATH на жодному з трьох хостів. На desk-tyan активні cad-cae-tunnel.service, eda-fpga-tunnel.service, hermes-gateway.service. На X99MG/dell відповідний фільтр user units не повернув рядків. Це не доводить відсутності system services/іншого binary path. Не перевстановлювати робоче підключення лише через PATH probe.

Рекомендована архітектура: один детермінований локальний worker із board lock та журналом; клієнти ШІ лише викликають дозволені typed tools. Запит моделі не дорівнює завершенню роботи. Worker повертає job_id, а клієнт читає bounded status/log/artifacts. Результат shell з невідомим станом спочатку узгодити, а не повторювати mutation.

## Мінімальний контракт нового PlutoSky MCP adapter (проект, не реалізований тут)
- status(host_id, board_id): read-only identity, profile, routes, active job, current gate.
- preflight(host_id, manifest_id): read-only checks; не записувати секрети у відповідь.
- submit(host_id, board_id, workflow, manifest_id, idempotency_key): лише зареєстровані workflow; enum monitor/ap/wfb-build/wfb-test/deploy.
- job_status(job_id), job_logs(job_id, cursor, limit), artifacts(job_id): bounded, redacted, hashes.
- cancel(job_id): зупинка власної process group і перевірений cleanup; cancellation не є rollback Flash.
- handoff(board_id, old_owner, new_owner): тільки після перевірки завершення попереднього owner.

Read tools позначати readOnlyHint; mutation tools не маскувати як read-only. Перевіряти authorization та board lock на сервері, не покладатися на prompt. Заборонити довільні shell/path/MTD аргументи у публічному API. Idempotency key прив’язати до manifest+workflow+board; повторний submit повертає існуючий job. Невідомий результат Flash не запускає повтор erase.

## ChatGPT web: послідовність
1. Перевірити наявні plugins/tools та виконати один справжній status виклик на потрібному host. Якщо чинний connector достатній — використовувати його.
2. Для нового private MCP спочатку локально перевірити фактичний HTTP або stdio upstream. Вибрати штатний Secure MCP Tunnel або дозволений HTTPS endpoint. Tailscale/SSH самі не є реєстрацією ChatGPT MCP.
3. Для Secure MCP Tunnel виявити точний встановлений binary path/version. Прочитати його --help, help quickstart та init --help; не копіювати вигадану схему профілю. Профіль має посилатися на реальний upstream/command, health лише loopback. Runtime key потрібного tunnel/workspace отримати через штатний account flow; локальний secret manager/0600, ніколи чат чи Git.
4. Виконати doctor для фактичного профілю. Permission/identity/key/workspace mismatch — BLOCKED, не retry loop. Перевірити user service, Restart=on-failure, NoNewPrivileges=true, UMask=0077, мережевий порядок. Після змін daemon-reload; active + health live/ready + успішні Control Plane polls; для unattended logout перевірити linger.
5. У ChatGPT web додати custom MCP plugin через доступні workspace controls: endpoint або Tunnel, authentication, встановлення plugin та явний вибір у чаті. Назви UI й доступність можуть змінитися; звірити офіційну документацію та реальні права workspace.
6. Перевірити discovery й прямий read-only tool call з унікальним run_id; host receipt має відповідати. Потім bounded harmless job, cancel/cleanup, повторний submit з тим самим idempotency key. Жодного Flash як першого інтеграційного smoke test.
7. Результат готовності: service/transport + discovered tools + успішний end-to-end call. Окремий health=ready недостатній. Не обіцяти unattended права, яких не дозволяє workspace.

Офіційні джерела перевірено 27.10.2026:
- https://developers.openai.com/api/docs/guides/custom-mcp-server
- https://developers.openai.com/api/docs/mcp
- Secure MCP Tunnel: перейти за актуальним посиланням з офіційної сторінки, перевірити client help перед установленням.

## Інші ШІ-сервіси та локальні CLI
Єдиний provider-neutral контракт: ті самі manifest, worker, lock, job_id, evidence та gates. Не запускати окремий deployment worker з кожного чату. Спочатку визначити, чи конкретний клієнт підтримує MCP HTTP/stdio або tool/function adapter, і перевірити актуальну офіційну документацію цього клієнта. Перелік сервісів користувачем не задано; універсальна автоматична авторизація тут не заявляється.

Для stdio-клієнта — локальний дозволений executable та env secret reference; для remote HTTP — перевірений endpoint і штатна auth. OpenAI tunnel не вважати універсальним транспортом стороннього сервісу без підтвердження. Для API function adapter сервер перевіряє JSON schema й передає запит у той самий worker. Зміна model/provider не змінює hardware state чи дозволи.

Codex/Hermes/OpenCode та інші CLI: спочатку command -v, --version, help і AGENTS.md. Не редагувати вручну приховані app directories всупереч host policy; використовувати штатні команди конфігурації. Не копіювати секрети між профілями. Не делегувати фізичні операції вкладеним CLI агентам і не запускати agents рекурсивно. Окремий watchdog/scheduler не створювати без доручення.

## Rate limits, audit та acceptance
Один board owner; максимум2 незалежні read calls, mutations послідовні. Великі логи читати cursor/tail, не цілком. Backoff/Retry-After, bounded retries; після transport timeout спершу status. Secret redaction до логування. Тестовий інтеграційний receipt містить host_id, run_id, server version, tool name, UTC time, manifest hash, результат; не ключі, raw captures чи повний env.
