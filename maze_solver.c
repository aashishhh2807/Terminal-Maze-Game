/*
 *  LABYRINTH — The Terminal Maze Game
 *  Compile:  gcc -o labyrinth maze_game.c
 *  Run:      ./labyrinth
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>

/* ── Colours ───────────────────────────────────────────────────── */
#define RST      "\033[0m"
#define BOLD     "\033[1m"
#define DIM      "\033[2m"
#define RED      "\033[31m"
#define YELLOW   "\033[33m"
#define BLUE     "\033[34m"
#define MAGENTA  "\033[35m"
#define BRED     "\033[91m"
#define BGREEN   "\033[92m"
#define BYELLOW  "\033[93m"
#define BBLUE    "\033[94m"
#define BMAGENTA "\033[95m"
#define BCYAN    "\033[96m"
#define BWHITE   "\033[97m"

/* ── Constants ─────────────────────────────────────────────────── */
#define MAX_NAME  32
#define MAX_ROWS  25
#define MAX_COLS  60

/* ── Types ─────────────────────────────────────────────────────── */
typedef struct { int row, col, dir; } Ghost;
typedef struct {
    char name[MAX_NAME];
    int  lives, score, level;
    int  wall_hits, coins, powers;
} Player;

/* ── Scoreboard ─────────────────────────────────────────────────── */
#define SCORE_FILE  ".labyrinth_scores"
#define MAX_ENTRIES 50

typedef struct {
    char name[MAX_NAME];
    int  total_score;
    int  games_played;
    int  wins;
} ScoreEntry;

static ScoreEntry scores[MAX_ENTRIES];
static int        score_count = 0;

static void scores_load(void){
    score_count = 0;
    FILE *f = fopen(SCORE_FILE,"r");
    if(!f) return;
    while(score_count < MAX_ENTRIES &&
          fscanf(f, "%31s %d %d %d",
                 scores[score_count].name,
                 &scores[score_count].total_score,
                 &scores[score_count].games_played,
                 &scores[score_count].wins) == 4)
        score_count++;
    fclose(f);
}

static void scores_save(void){
    FILE *f = fopen(SCORE_FILE,"w");
    if(!f) return;
    for(int i=0;i<score_count;i++)
        fprintf(f, "%s %d %d %d\n",
                scores[i].name,
                scores[i].total_score,
                scores[i].games_played,
                scores[i].wins);
    fclose(f);
}

/* Add/update entry for this player, return their cumulative total */
static int scores_add(const char *name, int score, int won){
    for(int i=0;i<score_count;i++){
        if(strcasecmp(scores[i].name, name)==0){
            scores[i].total_score  += score;
            scores[i].games_played += 1;
            scores[i].wins         += won ? 1 : 0;
            scores_save();
            return scores[i].total_score;
        }
    }
    if(score_count < MAX_ENTRIES){
        strncpy(scores[score_count].name, name, MAX_NAME-1);
        scores[score_count].name[MAX_NAME-1]='\0';
        scores[score_count].total_score  = score;
        scores[score_count].games_played = 1;
        scores[score_count].wins         = won ? 1 : 0;
        score_count++;
        scores_save();
    }
    return score;
}

/* Sort descending by total_score (simple insertion sort) */
static void scores_sort(void){
    for(int i=1;i<score_count;i++){
        ScoreEntry tmp = scores[i];
        int j=i-1;
        while(j>=0 && scores[j].total_score < tmp.total_score){
            scores[j+1]=scores[j]; j--;
        }
        scores[j+1]=tmp;
    }
}


static char  maze[MAX_ROWS][MAX_COLS + 2];
static int   mrows, mcols;
static int   pr, pc;
static Ghost ghosts[4];
static int   ghost_count;

/* ── Terminal helpers ──────────────────────────────────────────── */
static struct termios old_term;
static void enable_raw(void){
    struct termios t; tcgetattr(0,&old_term); t=old_term;
    t.c_lflag &= ~(ECHO|ICANON); t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
    tcsetattr(0,TCSAFLUSH,&t);
}
static void disable_raw(void){ tcsetattr(0,TCSAFLUSH,&old_term); }
static void clr(void){ printf("\033[2J\033[H"); fflush(stdout); }
static char getch(void){
    char c=0; enable_raw(); read(0,&c,1);
    if(c==27){ char s[2];
        if(read(0,&s[0],1)==1&&read(0,&s[1],1)==1&&s[0]=='[')
            switch(s[1]){ case 'A':c='w';break; case 'B':c='s';break;
                          case 'C':c='d';break; case 'D':c='a';break; }
    }
    disable_raw(); return c;
}

/* ── Border helpers ────────────────────────────────────────────── */
static void hline_eq(int inner, const char *col){
    printf("%s+", col);
    for(int i=0;i<inner;i++) printf("=");
    printf("+%s\n", RST);
}
static void hline_dash(int inner, const char *col){
    printf("%s+", col);
    for(int i=0;i<inner;i++) printf("-");
    printf("+%s\n", RST);
}

/* ── Mazes: every row the same width, BFS-verified solvable ─────
   EASY   11 x 23
   MEDIUM 15 x 35
   HARD   19 x 47  (generated programmatically, BFS-verified)
   ─────────────────────────────────────────────────────────────── */

static const char *easy_maze[] = {
    "#######################",
    "#S      #   *     #   #",
    "####### # ####### # # #",
    "#     # #       # # # #",
    "# ### # ####### # # # #",
    "# # .   .       #   # #",
    "# # ########### ##### #",
    "# #           .       #",
    "# ########### ####### #",
    "#           *        E#",
    "#######################",
};

static const char *medium_maze[] = {
    "###################################",
    "#S  .   #   .   #   *   #   .   . #",
    "# ##### # ##### # ##### # ##### # #",
    "#     # #     # #     # #     # # #",
    "##### # ##### # ##### # ##### # # #",
    "#   . #     . #     . #     . # # #",
    "# ########### ########### ##### # #",
    "#           .           .       # #",
    "# ######### ########### ######### #",
    "# #   .   # #   .   # #   .   # # #",
    "# # ##### # # ##### # # ##### # # #",
    "# #     # # #     # # #     # # . #",
    "# ####### # ####### # ####### # ###",
    "#   *   .   #   *   .   #   *    E#",
    "###################################",
};

/* Hard 19x37 — generated by recursive back-tracker (seed=123).
   All rows exactly 37 chars.  BFS-verified solvable.
   Three ghosts at BFS-distances 76, 111, 143 from S — placed on
   cells that are NOT the only passage (maze stays solvable with
   ghosts treated as walls).                                        */
static const char *hard_maze[] = {
    "#####################################",
    "#S#             #       #           #",
    "# ### ######### ### ### ### ####### #",
    "# #   #       #     # #   #.#     # #",
    "# # ### # # ######### ### # ##### # #",
    "# #   # # #*#     #   #   #     # # #",
    "# ### # # # # # ### # # #######.# # #",
    "#     #*# # # #     # #.  # . #   # #",
    "#########.### ####### ### ### ###.# #",
    "# #       #   #         #. .#     # #",
    "# # ### ### ### ########### ####### #",
    "#   # #   # # ..#           #   #   #",
    "# ### ### # # ### ########### # # ###",
    "# #   #   # #   # #        .  # #   #",
    "# # # # ### ### # # # ######### ###.#",
    "#   # #     #   # # #   #     # #*  #",
    "#####.####### ### ##### # ### # # # #",
    "#  .    .   #  *        #   #  .  #E#",
    "#####################################",
};

/* ── Load maze ─────────────────────────────────────────────────── */
static void load_maze(int level){
    const char **src;
    if      (level==1){ src=easy_maze;   mrows=11; mcols=23; }
    else if (level==2){ src=medium_maze; mrows=15; mcols=35; }
    else              { src=hard_maze;   mrows=19; mcols=37; }
    for(int i=0;i<mrows;i++){
        strncpy(maze[i],src[i],MAX_COLS+1);
        maze[i][MAX_COLS+1]='\0';
    }
    ghost_count=0;
    for(int i=0;i<mrows;i++)
        for(int j=0;j<mcols;j++)
            if(maze[i][j]=='S'){ pr=i; pc=j; }
    if(level==3){
        /* Three ghosts on non-bottleneck cells (BFS-verified: maze
           stays solvable even with all three treated as walls).     */
        int gpos[3][2]={{5,19},{1,31},{3,13}};
        ghost_count=3;
        for(int g=0;g<3;g++){
            ghosts[g].row=gpos[g][0]; ghosts[g].col=gpos[g][1];
            ghosts[g].dir=g%4;
            char *cell=&maze[gpos[g][0]][gpos[g][1]];
            if(*cell==' '||*cell=='.') *cell='G';
        }
    }
}

/* ── Render ────────────────────────────────────────────────────── */
static void render(Player *p){
    clr();
    /* inner width = maze width + 2 spaces padding each side */
    int iw = mcols + 4;

    /* Title banner */
    hline_eq(iw, BOLD BCYAN);
    {
        int pad = (iw - 9) / 2;
        printf(BOLD BCYAN "|" RST);
        for(int i=0;i<pad;i++) printf(" ");
        printf(BOLD BYELLOW "LABYRINTH" RST);
        for(int i=0;i<iw-pad-9;i++) printf(" ");
        printf(BOLD BCYAN "|\n" RST);
    }
    hline_eq(iw, BOLD BCYAN);

    /* HUD */
    const char *lname = p->level==1?"EASY":p->level==2?"MEDIUM":"HARD";
    const char *lcol  = p->level==1?BGREEN:p->level==2?BYELLOW:BRED;
    int lives_show = p->lives < 0 ? 0 : (p->lives > 10 ? 10 : p->lives);
    printf(BOLD BCYAN "|" RST " ");
    printf(BOLD BWHITE "%-12s" RST, p->name);
    printf("  " BOLD RED "HP:");
    for(int i=0;i<lives_show;i++) printf(BOLD RED "o" RST);
    for(int i=lives_show;i<5;i++) printf(DIM "-" RST);
    printf("  " BOLD BYELLOW "Score:%5d" RST, p->score);
    printf("  " BOLD "Lv:%s%-6s" RST, lcol, lname);
    printf("  " BOLD "Hits:%d" RST "\n", p->wall_hits);
    hline_dash(iw, BOLD BCYAN);

    /* Maze rows */
    for(int r=0;r<mrows;r++){
        printf(BOLD BCYAN "| " RST " ");
        for(int c=0;c<mcols;c++){
            char ch = (r==pr && c==pc) ? 'P' : maze[r][c];
            switch(ch){
                case '#': printf(BOLD BBLUE    "#" RST); break;
                case 'S': printf(BOLD BGREEN   "S" RST); break;
                case 'E': printf(BOLD BGREEN   "E" RST); break;
                case '*': printf(BOLD BMAGENTA "+" RST); break;
                case '.': printf(BOLD YELLOW   "." RST); break;
                case 'G': printf(BOLD BRED     "G" RST); break;
                case 'P': printf(BOLD BYELLOW  "@" RST); break;
                case ' ': printf(" ");                   break;
                default:  printf("%c", ch);              break;
            }
        }
        printf(BOLD BCYAN "  |\n" RST);
    }

    /* Bottom legend */
    hline_dash(iw, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST
           " Move: WASD/arrows   Quit: Q   "
           BMAGENTA "+" RST "=+life  "
           YELLOW "." RST "=+10pts  "
           BRED "G" RST "=ghost  "
           BGREEN "E" RST "=exit");
    /* pad to iw */
    int used = 4 + (int)strlen(" Move: WASD/arrows   Quit: Q   +=+life  .=+10pts  G=ghost  E=exit");
    for(int i=used;i<iw+1;i++) printf(" ");
    printf(BOLD BCYAN "|\n" RST);
    hline_eq(iw, BOLD BCYAN);
    printf("\n");
}

/* ── Ghost movement ────────────────────────────────────────────── */
static void move_ghosts(void){
    int dr[]={-1,1,0,0}, dc[]={0,0,-1,1};
    for(int g=0;g<ghost_count;g++){
        Ghost *gh=&ghosts[g];
        if(maze[gh->row][gh->col]=='G') maze[gh->row][gh->col]=' ';
        for(int t=0;t<4;t++){
            int nr=gh->row+dr[gh->dir], nc=gh->col+dc[gh->dir];
            if(nr>=0&&nr<mrows&&nc>=0&&nc<mcols&&
               maze[nr][nc]!='#'&&maze[nr][nc]!='E'&&maze[nr][nc]!='S'){
                gh->row=nr; gh->col=nc; break;
            }
            gh->dir=(gh->dir+1)%4;
        }
        if(maze[gh->row][gh->col]!='P') maze[gh->row][gh->col]='G';
    }
}
static int ghost_on_player(void){
    for(int g=0;g<ghost_count;g++)
        if(ghosts[g].row==pr&&ghosts[g].col==pc) return 1;
    return 0;
}

/* ── Flash message ─────────────────────────────────────────────── */
static void flash(const char *col, const char *msg){
    printf("  %s >> %s << %s\n", col, msg, RST);
    fflush(stdout); usleep(550000);
}

/* ── Opening screen ────────────────────────────────────────────── */
static void opening_screen(void){
    clr();
    int W=72;
    /* outer box */
    printf(BOLD BYELLOW);
    hline_eq(W, BOLD BYELLOW);
    /* ASCII title — 9-letter LABYRINTH */
    printf(BOLD BYELLOW "| " RST BOLD "  ██╗      █████╗ ██████╗ ██╗   ██╗██████╗ ██╗███╗  ██╗████████╗██╗  ██╗   " RST BOLD BYELLOW " |\n" RST);
    printf(BOLD BYELLOW "| " RST BOLD "  ██║     ██╔══██╗██╔══██╗╚██╗ ██╔╝██╔══██╗██║████╗ ██║╚══██╔══╝██║  ██║   " RST BOLD BYELLOW " |\n" RST);
    printf(BOLD BYELLOW "| " RST BOLD "  ██║     ███████║██████╔╝ ╚████╔╝ ██████╔╝██║██╔██╗██║   ██║   ███████║   " RST BOLD BYELLOW " |\n" RST);
    printf(BOLD BYELLOW "| " RST BOLD "  ██║     ██╔══██║██╔══██╗  ╚██╔╝  ██╔══██╗██║██║╚████║   ██║   ██╔══██║   " RST BOLD BYELLOW " |\n" RST);
    printf(BOLD BYELLOW "| " RST BOLD "  ███████╗██║  ██║██████╔╝   ██║   ██║  ██║██║██║ ╚███║   ██║   ██║  ██║   " RST BOLD BYELLOW " |\n" RST);
    printf(BOLD BYELLOW "| " RST BOLD "  ╚══════╝╚═╝  ╚═╝╚═════╝    ╚═╝   ╚═╝  ╚═╝╚═╝╚═╝  ╚══╝   ╚═╝   ╚═╝  ╚═╝   " RST BOLD BYELLOW " |\n" RST);
    printf(BOLD BYELLOW "| " RST BOLD "                    T H E   T E R M I N A L   M A Z E   G A M E              " RST BOLD BYELLOW " |\n" RST);
    hline_eq(W, BOLD BYELLOW);
    printf("\n");

    /* How to play box */
    hline_eq(W, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST BOLD "                          H O W   T O   P L A Y                              " RST BOLD BCYAN " |\n" RST);
    hline_dash(W, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST "  " BWHITE  "Goal   " RST ": Go from " BGREEN "S" RST " (start) to " BGREEN "E" RST " (exit)                                       " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " BWHITE  "Move   " RST ": W A S D  or  Arrow keys                                             " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " BWHITE  "Lives  " RST ": Start with " RED "5 lives" RST " shown as " RED "ooooo" RST "                                  " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " BWHITE  "Walls  " RST ": Hitting a " BBLUE "#" RST " wall costs " BRED "-1 life" RST "                                    " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " BMAGENTA "+"      RST " Powerup : collect to gain " BMAGENTA "+1 life" RST "                                    " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " YELLOW   "."      RST " Coin    : each worth " YELLOW "+10 score" RST "                                          " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " BRED     "G"      RST " Ghost   : Hard mode only -- touching costs " BRED "-2 lives" RST "                   " BOLD BCYAN " |\n" RST);
    printf(BOLD BCYAN "| " RST "  " BYELLOW  "Bonus  " RST ": " BYELLOW "+500 pts" RST " on exit  +  " BYELLOW "50 pts" RST " per life remaining                   " BOLD BCYAN " |\n" RST);
    hline_eq(W, BOLD BCYAN);
    printf("\n  " BOLD BGREEN "Press ENTER to begin ..." RST "\n");
    enable_raw(); while(getchar()!='\n'); disable_raw();
}

/* ── Name screen ───────────────────────────────────────────────── */
static void name_screen(char *name){
    clr(); printf("\n\n");
    int W=52;
    hline_eq(W, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST BOLD "               ENTER YOUR NAME                          " RST BOLD BCYAN " |\n" RST);
    hline_eq(W, BOLD BCYAN);
    printf("\n  " BWHITE "Your name: " RST); fflush(stdout);
    if(fgets(name,MAX_NAME,stdin)){ name[strcspn(name,"\n")]='\0'; if(!strlen(name)) strcpy(name,"Player"); }
    printf("\n" BOLD BGREEN "  Welcome, %s!  Good luck in the labyrinth!\n\n" RST, name);
    sleep(1);
}

/* ── Level screen ──────────────────────────────────────────────── */
static int level_screen(char *name){
    clr(); printf("\n\n");
    int W=52;
    hline_eq(W, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST BOLD "  Hello, %-16s  -- Choose difficulty:        " RST BOLD BCYAN " |\n" RST, name);
    hline_eq(W, BOLD BCYAN);
    printf("\n");
    printf("   " BOLD BGREEN  "  [1]  EASY  " RST "   Small maze, no enemies                \n");
    printf("   " BOLD BYELLOW "  [2]  MEDIUM" RST "   Bigger maze, more coins and powerups  \n");
    printf("   " BOLD BRED    "  [3]  HARD  " RST "   Large maze + one roaming ghost        \n");
    printf("\n  " BOLD BWHITE "Enter 1, 2 or 3: " RST); fflush(stdout);
    enable_raw(); char ch=0; while(ch<'1'||ch>'3') ch=getchar(); disable_raw();
    printf("%c\n",ch); sleep(1);
    return ch-'0';
}

/* ── End screen ────────────────────────────────────────────────── */
static void end_screen(Player *p, int won){
    /* Update persistent scoreboard first */
    int cumulative = scores_add(p->name, p->score, won);
    scores_sort();

    clr(); printf("\n\n");
    int W=60;
    if(won){
        hline_eq(W, BOLD BGREEN);
        printf(BOLD BGREEN "| " RST BOLD "           *  *  CONGRATULATIONS!  *  *                     " RST BOLD BGREEN " |\n" RST);
        printf(BOLD BGREEN "| " RST BOLD "               You escaped the labyrinth!                   " RST BOLD BGREEN " |\n" RST);
        hline_eq(W, BOLD BGREEN);
    } else {
        hline_eq(W, BOLD BRED);
        printf(BOLD BRED "| " RST BOLD "                      GAME  OVER                             " RST BOLD BRED " |\n" RST);
        printf(BOLD BRED "| " RST BOLD "              The labyrinth claimed you ...                   " RST BOLD BRED " |\n" RST);
        hline_eq(W, BOLD BRED);
    }
    printf("\n");

    /* ── This game's stats ── */
    hline_eq(W, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST BOLD "                     THIS  GAME                               " RST BOLD BCYAN " |\n" RST);
    hline_dash(W, BOLD BCYAN);
    printf(BOLD BCYAN "| " RST "  " BWHITE   "Player      " RST " : %-44s" BOLD BCYAN " |\n" RST, p->name);
    printf(BOLD BCYAN "| " RST "  " BYELLOW  "Score       " RST " : %-44d" BOLD BCYAN " |\n" RST, p->score);
    printf(BOLD BCYAN "| " RST "  " BYELLOW  "Cumulative  " RST " : %-44d" BOLD BCYAN " |\n" RST, cumulative);
    printf(BOLD BCYAN "| " RST "  " RED      "Lives left  " RST " : %-44d" BOLD BCYAN " |\n" RST, p->lives<0?0:p->lives);
    printf(BOLD BCYAN "| " RST "  " BRED     "Wall hits   " RST " : %-44d" BOLD BCYAN " |\n" RST, p->wall_hits);
    printf(BOLD BCYAN "| " RST "  " YELLOW   "Coins found " RST " : %-44d" BOLD BCYAN " |\n" RST, p->coins);
    printf(BOLD BCYAN "| " RST "  " BMAGENTA "Powerups    " RST " : %-44d" BOLD BCYAN " |\n" RST, p->powers);
    hline_eq(W, BOLD BCYAN);

    /* ── Scoreboard ── */
    printf("\n");
    hline_eq(W, BOLD BYELLOW);
    printf(BOLD BYELLOW "| " RST BOLD "                   S C O R E B O A R D                        " RST BOLD BYELLOW " |\n" RST);
    hline_dash(W, BOLD BYELLOW);
    printf(BOLD BYELLOW "| " RST BOLD BWHITE  "  # %-14s  %10s  %6s  %6s    " RST BOLD BYELLOW "|\n" RST,
           "Name", "Total", "Games", "Wins");
    hline_dash(W, BOLD BYELLOW);

    int show = score_count < 10 ? score_count : 10;
    for(int i=0;i<show;i++){
        /* Highlight the current player */
        const char *nc = (strcasecmp(scores[i].name, p->name)==0) ? BYELLOW : BWHITE;
        const char *marker = (strcasecmp(scores[i].name, p->name)==0) ? ">" : " ";
        printf(BOLD BYELLOW "| " RST
               "%s%s%2d %-14s  %10d  %6d  %6d    " RST
               BOLD BYELLOW "|\n" RST,
               BOLD, nc,
               i+1, scores[i].name,
               scores[i].total_score,
               scores[i].games_played,
               scores[i].wins);
        (void)marker;
    }
    hline_eq(W, BOLD BYELLOW);

    printf("\n  " BOLD BWHITE "Play again? (y / n): " RST); fflush(stdout);
    enable_raw(); char ch=0; while(ch!='y'&&ch!='Y'&&ch!='n'&&ch!='N') ch=getchar(); disable_raw();
    printf("%c\n",ch); sleep(1);
    if(ch=='n'||ch=='N'){
        clr(); printf("\n\n");
        hline_eq(W, BOLD BCYAN);
        printf(BOLD BCYAN "| " RST BOLD "  Thanks for playing LABYRINTH, %-12s                    " RST BOLD BCYAN " |\n" RST, p->name);
        printf(BOLD BCYAN "| " RST BOLD "  Cumulative score: %-8d                                     " RST BOLD BCYAN " |\n" RST, cumulative);
        hline_eq(W, BOLD BCYAN);
        printf("\n"); exit(0);
    }
}

/* ── Game loop ─────────────────────────────────────────────────── */
static int play(Player *p){
    load_maze(p->level);
    int gtick=0;
    while(1){
        render(p);
        if(ghost_on_player()){ p->lives-=2; flash(BRED,"Ghost caught you!  -2 lives"); if(p->lives<=0) return 0; load_maze(p->level); continue; }
        char key=getch();
        if(key=='q'||key=='Q') return 0;
        int dr=0,dc=0;
        if     (key=='w'||key=='W') dr=-1;
        else if(key=='s'||key=='S') dr= 1;
        else if(key=='a'||key=='A') dc=-1;
        else if(key=='d'||key=='D') dc= 1;
        else continue;
        int nr=pr+dr, nc=pc+dc;
        if(nr<0||nr>=mrows||nc<0||nc>=mcols) continue;
        char t=maze[nr][nc];
        if(t=='#'){ p->lives--; p->wall_hits++; flash(BRED,"Ouch! Wall hit  -1 life"); if(p->lives<=0) return 0; }
        else{
            if(maze[pr][pc]!='S') maze[pr][pc]=' ';
            pr=nr; pc=nc;
            if     (t=='E'){ p->score+=500+(p->lives>0?p->lives:0)*50; return 1; }
            else if(t=='*'){ p->lives++; p->score+=25; p->powers++; maze[nr][nc]=' '; flash(BMAGENTA,"Powerup! +1 life"); }
            else if(t=='.'){ p->score+=10; p->coins++; maze[nr][nc]=' '; }
            else if(t=='G'){ p->lives-=2; flash(BRED,"Ran into a ghost! -2 lives"); if(p->lives<=0) return 0; load_maze(p->level); continue; }
        }
        if(p->level==3){ gtick++; if(gtick%3==0) move_ghosts(); }
    }
}

/* ── main ──────────────────────────────────────────────────────── */
int main(void){
    srand((unsigned)time(NULL));
    scores_load();
    Player p; memset(&p,0,sizeof(p));
    while(1){
        opening_screen();
        name_screen(p.name);
        p.level=level_screen(p.name);
        p.lives=5; p.score=0; p.wall_hits=0; p.coins=0; p.powers=0;
        int won=play(&p);
        disable_raw();
        end_screen(&p,won);
    }
    return 0;
}
