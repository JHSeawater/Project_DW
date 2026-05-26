
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#define MIN_WIDTH 100
#define MIN_HEIGHT 30
#define INPUT_MAX 256
#define QUICK_HELP "[ /fake rumor | /fake raid | /fake ok | /fake over | /quit ]"

#define PKT_EVT_RUMOR_GLITCH 305
#define PKT_EVT_POLICE_RAID 306
#define PKT_EVT_GAME_OVER 307
#define PKT_RES_MINIGAME_OK 208

typedef enum {
    STATE_NORMAL,
    STATE_PANIC,
    STATE_GAME_OVER
} ClientState;

typedef struct {
    int type;
    int value;
    unsigned int tag_mask;
    char text[256];
} Packet;

typedef struct {
    WINDOW *news;
    WINDOW *market;
    WINDOW *bounty;
    WINDOW *inventory;
    WINDOW *log;
    WINDOW *prompt;
} UI;

volatile sig_atomic_t is_panic = 0;

void safe_endwin(void) {
    if (!isendwin()) {
        endwin();
    }
}

void handle_sigint(int sig) {
    is_panic = 1;
}

void delete_windows(UI *ui) {
    if (ui->news) delwin(ui->news);
    if (ui->market) delwin(ui->market);
    if (ui->bounty) delwin(ui->bounty);
    if (ui->inventory) delwin(ui->inventory);
    if (ui->log) delwin(ui->log);
    if (ui->prompt) delwin(ui->prompt);
    memset(ui, 0, sizeof(UI));
}

int check_size(void) {
    int h, w;
    getmaxyx(stdscr, h, w);

    if (w < MIN_WIDTH || h < MIN_HEIGHT) {
        clear();
        mvprintw(h / 2, 2, "해상도를 더 키우고 실행해 주세요. 현재 %dx%d / 목표 %dx%d",
                 w, h, MIN_WIDTH, MIN_HEIGHT);
        refresh();
        getch();
        return 0;
    }
    return 1;
}

void draw_title(WINDOW *win, const char *title) {
    box(win, 0, 0);
    mvwprintw(win, 0, 2, " %s ", title);
}

void get_time_string(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    snprintf(buf, size, "%02d:%02d:%02d",
             t->tm_hour, t->tm_min, t->tm_sec);
}

void log_message(UI *ui, const char *msg) {
    char time_buf[16];
    get_time_string(time_buf, sizeof(time_buf));

    wprintw(ui->log, "\n[%s] %s", time_buf, msg);
    wrefresh(ui->log);
}

const char *get_tag_name(int idx) {
    static const char *tags[32] = {
        "B기업", "재무제표", "국방부", "기밀도면",
        "민간", "주민번호", "A정치인", "녹취록",
        "GPS좌표", "비자금", "고객정보", "군사무기",
        "인사발령", "보안", "해외", "용병단"
    };

    if (idx >= 0 && idx < 16) {
        return tags[idx];
    }
    return "UNKNOWN";
}

void render_tags(WINDOW *win, int y, int x, unsigned int mask) {
    int first = 1;

    wmove(win, y, x);

    for (int i = 0; i < 32; i++) {
        if (mask & (1U << i)) {
            if (!first) {
                wprintw(win, "+");
            }
            wprintw(win, "[%s]", get_tag_name(i));
            first = 0;
        }
    }

    if (first) {
        wprintw(win, "[NO_TAG]");
    }
}

void restore_prompt_cursor(UI *ui, const char *input, int len) {
    int h, w;
    int help_len = strlen(QUICK_HELP);

    getmaxyx(stdscr, h, w);

    werase(ui->prompt);
    mvwprintw(ui->prompt, 0, 0, "> %s", input);

    if (w > help_len + 2) {
        mvwprintw(ui->prompt, 0, w - help_len - 1, "%s", QUICK_HELP);
    }

    wmove(ui->prompt, 0, len + 2);
    wrefresh(ui->prompt);
}

int input_limit_width(void) {
    int h, w;
    getmaxyx(stdscr, h, w);

    return w - strlen(QUICK_HELP) - 5;
}

void draw_ui(UI *ui) {
    int h, w;
    int y = 0;

    getmaxyx(stdscr, h, w);
    clear();
    refresh();

    int news_h = 4;
    int prompt_h = 1;
    int inventory_h = 5;
    int middle_h = h * 45 / 100;
    int log_h = h - news_h - middle_h - inventory_h - prompt_h;

    int market_w = w * 50 / 100;
    int bounty_w = w - market_w;

    ui->news = newwin(news_h, w, y, 0);
    y += news_h;

    ui->market = newwin(middle_h, market_w, y, 0);
    ui->bounty = newwin(middle_h, bounty_w, y, market_w);
    y += middle_h;

    ui->inventory = newwin(inventory_h, w, y, 0);
    y += inventory_h;

    ui->log = newwin(log_h, w, y, 0);
    y += log_h;

    ui->prompt = newwin(prompt_h, w, y, 0);

    keypad(ui->prompt, TRUE);
    nodelay(ui->prompt, TRUE);

    scrollok(ui->log, TRUE);
    scrollok(ui->prompt, FALSE);

    draw_title(ui->news, "다크웹 중앙 뉴스망 / 브로드캐스트 브리핑");
    mvwprintw(ui->news, 1, 2, "> [속보] A정치인 비자금 스캔들 관련 기밀 가치 상승 전망");
    mvwprintw(ui->news, 2, 2, "> [SYSTEM] Ctrl+C 입력 시 긴급 파기 모드 진입");

    draw_title(ui->market, "글로벌 마켓");
    mvwprintw(ui->market, 1, 2, "ID  | 태그 조합 및 기밀 정보        | 가치");
    mvwprintw(ui->market, 2, 2, "201 | ");
    render_tags(ui->market, 2, 8, (1U << 0) | (1U << 1));
    mvwprintw(ui->market, 2, 38, "| $300");

    mvwprintw(ui->market, 3, 2, "202 | ");
    render_tags(ui->market, 3, 8, (1U << 2) | (1U << 3));
    mvwprintw(ui->market, 3, 38, "| $500");

    draw_title(ui->bounty, "VIP NPC 의뢰 보드");
    mvwprintw(ui->bounty, 1, 2, "#011 익명 공매도 세력");
    mvwprintw(ui->bounty, 2, 2, "[목표]: ");
    render_tags(ui->bounty, 2, 10, (1U << 0) | (1U << 1));
    mvwprintw(ui->bounty, 3, 2, "[보상]: $1,200");

    draw_title(ui->inventory, "내 샌드박스 인벤토리");
    mvwprintw(ui->inventory, 1, 2, "[01] ID:088 | ");
    render_tags(ui->inventory, 1, 18, (1U << 2) | (1U << 8));
    mvwprintw(ui->inventory, 2, 2, "[02] ID:154 | ");
    render_tags(ui->inventory, 2, 18, (1U << 6) | (1U << 9));

    draw_title(ui->log, "시스템 알림 및 채팅 로그");
    mvwprintw(ui->log, 1, 2, "[SYSTEM] UI Signal Engine started.");
    mvwprintw(ui->log, 2, 2, "[TEST] /fake rumor, /fake raid, /fake ok, /fake over");

    restore_prompt_cursor(ui, "", 0);

    wrefresh(ui->news);
    wrefresh(ui->market);
    wrefresh(ui->bounty);
    wrefresh(ui->inventory);
    wrefresh(ui->log);
    wrefresh(ui->prompt);
}

void redraw(UI *ui) {
    delete_windows(ui);
    clear();
    refresh();

    if (!check_size()) {
        safe_endwin();
        exit(0);
    }

    draw_ui(ui);
}

void render_glitch(UI *ui) {
    int h, w;
    getmaxyx(ui->bounty, h, w);

    start_color();
    use_default_colors();

    init_pair(1, COLOR_RED, -1);
    init_pair(2, COLOR_GREEN, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_YELLOW, -1);

    srand(time(NULL));

    for (int i = 1; i < h - 1; i++) {
        int color = 1 + rand() % 5;
        int x = 2 + rand() % (w - 10);

        wattron(ui->bounty, COLOR_PAIR(color));
        wattron(ui->bounty, A_REVERSE);

        mvwprintw(ui->bounty, i, x, "#@!%c%c%d",
                  'A' + rand() % 26,
                  'a' + rand() % 26,
                  rand() % 100);

        wattroff(ui->bounty, A_REVERSE);
        wattroff(ui->bounty, COLOR_PAIR(color));
    }

    wrefresh(ui->bounty);
    log_message(ui, "[경고] /rumor 사보타주로 VIP 보드가 손상되었습니다.");
}

void render_panic_mode(UI *ui) {
    clear();

    attron(A_REVERSE);
    mvprintw(3, 5, "!!! POLICE RAID DETECTED !!!");
    attroff(A_REVERSE);

    mvprintw(5, 5, "긴급 파기 모드 진입");
    mvprintw(7, 5, "암호를 입력하세요: PURGE");
    mvprintw(9, 5, "성공 테스트 명령어: /fake ok");
    mvprintw(11, 5, "주의: 현재 상태에서는 마켓 입력이 잠깁니다.");

    refresh();
}

void render_game_over(const char *msg) {
    clear();

    attron(A_REVERSE);
    mvprintw(LINES / 2 - 1, COLS / 2 - 10, " GAME OVER ");
    attroff(A_REVERSE);

    mvprintw(LINES / 2 + 1, COLS / 2 - 20, "%s", msg);
    mvprintw(LINES / 2 + 3, COLS / 2 - 20, "아무 키나 누르면 종료됩니다.");

    refresh();
    getch();

    safe_endwin();
    exit(0);
}

void handle_packet(UI *ui, Packet *pkt, ClientState *state) {
    if (pkt->type == PKT_EVT_RUMOR_GLITCH) {
        render_glitch(ui);
    }
    else if (pkt->type == PKT_EVT_POLICE_RAID) {
        *state = STATE_PANIC;
        render_panic_mode(ui);
    }
    else if (pkt->type == PKT_RES_MINIGAME_OK) {
        *state = STATE_NORMAL;
        redraw(ui);
        log_message(ui, "[SYSTEM] 긴급 파기 성공. 정상 마켓 화면으로 복귀합니다.");
    }
    else if (pkt->type == PKT_EVT_GAME_OVER) {
        *state = STATE_GAME_OVER;
        render_game_over(pkt->text);
    }
}

void make_fake_packet(Packet *pkt, int type) {
    memset(pkt, 0, sizeof(Packet));
    pkt->type = type;

    if (type == PKT_EVT_GAME_OVER) {
        strcpy(pkt->text, "패배: 경찰 레이드 회피에 실패했습니다.");
    }
}

int main(void) {
    UI ui = {0};
    ClientState state = STATE_NORMAL;

    char input[INPUT_MAX];
    int len = 0;
    int running = 1;

    atexit(safe_endwin);
    signal(SIGINT, handle_sigint);

    setlocale(LC_ALL, "");

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    timeout(0);

    memset(input, 0, sizeof(input));

    if (!check_size()) {
        safe_endwin();
        return 1;
    }

    draw_ui(&ui);

    while (running) {
        if (is_panic) {
            is_panic = 0;
            state = STATE_PANIC;
            render_panic_mode(&ui);
            len = 0;
            memset(input, 0, sizeof(input));
        }

        int ch = wgetch(ui.prompt);

        if (ch == ERR) {
            continue;
        }

        if (ch == KEY_RESIZE) {
            if (state == STATE_NORMAL) {
                redraw(&ui);
                restore_prompt_cursor(&ui, input, len);
            } else if (state == STATE_PANIC) {
                render_panic_mode(&ui);
            }
            continue;
        }

        if (ch == '\n') {
            input[len] = '\0';

            Packet pkt;

            if (strcmp(input, "/quit") == 0) {
                running = 0;
            }
            else if (strcmp(input, "/fake rumor") == 0) {
                make_fake_packet(&pkt, PKT_EVT_RUMOR_GLITCH);
                handle_packet(&ui, &pkt, &state);
            }
            else if (strcmp(input, "/fake raid") == 0) {
                make_fake_packet(&pkt, PKT_EVT_POLICE_RAID);
                handle_packet(&ui, &pkt, &state);
            }
            else if (strcmp(input, "/fake ok") == 0) {
                make_fake_packet(&pkt, PKT_RES_MINIGAME_OK);
                handle_packet(&ui, &pkt, &state);
            }
            else if (strcmp(input, "/fake over") == 0) {
                make_fake_packet(&pkt, PKT_EVT_GAME_OVER);
                handle_packet(&ui, &pkt, &state);
            }
            else if (strcmp(input, "/tags") == 0) {
                log_message(&ui, "[SYSTEM] 다중 태그 비트마스크 렌더링 테스트 완료");
            }
            else {
                char msg[INPUT_MAX + 16];
                snprintf(msg, sizeof(msg), "[YOU] %s", input);
                log_message(&ui, msg);
            }

            len = 0;
            memset(input, 0, sizeof(input));

            if (state == STATE_NORMAL) {
                restore_prompt_cursor(&ui, input, len);
            }
            continue;
        }

        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (len > 0) {
                len--;
                input[len] = '\0';
            }
        }
        else if (ch == 27) {
            len = 0;
            memset(input, 0, sizeof(input));
        }
        else if (ch >= 32 && ch <= 126) {
            int limit = input_limit_width();

            if (len < INPUT_MAX - 1 && len < limit) {
                input[len++] = ch;
                input[len] = '\0';
            }
        }

        if (state == STATE_NORMAL) {
            restore_prompt_cursor(&ui, input, len);
        } else if (state == STATE_PANIC) {
            mvprintw(13, 5, "> %s", input);
            clrtoeol();
            refresh();
        }
    }

    delete_windows(&ui);
    safe_endwin();

    printf("UNDERFLOW UI safely closed.\n");
    return 0;
}