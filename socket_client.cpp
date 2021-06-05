#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stack>
#include <thread>
#include <iostream>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>

using namespace std;

#define MEMORY_SPACE 2028

WINDOW *mainWin, *inputWin, *chatWin, *chatWinBox, *inputWinBox;

/*
Using n-curses library as dependency
*/

/* Function to define colors of prompt*/
void colors()
{
  // Most terminals support 8 colors
  // COLOR_BLACK
  // COLOR_BLUE
  // COLOR_GREEN
  // COLOR_CYAN
  // COLOR_RED
  // COLOR_MAGENTA
  // COLOR_YELLOW
  // COLOR_WHITE
  // init_pair(#, [text color], [background color])

  // Tell curses we're using colors
  start_color();
  // Start using the default terminal colors
  use_default_colors();

  // Initialize color pairs
  init_pair(1, -1, -1); // Default
  init_pair(2, COLOR_CYAN, -1);
  init_pair(3, COLOR_YELLOW, -1);
  init_pair(4, COLOR_RED, -1);
  init_pair(5, COLOR_BLUE, -1);
  init_pair(6, COLOR_MAGENTA, -1);
  init_pair(7, COLOR_GREEN, -1);
  init_pair(8, COLOR_WHITE, COLOR_RED);
  //init_pair(9, COLOR_WHITE, COLOR_BLUE);
  //init_pair(10, COLOR_WHITE, COLOR_GREEN);
  //init_pair(11, COLOR_BLACK, COLOR_MAGENTA);
}

/* Draw chat box and window */
void drawChatWin()
{
  // Create window for chat box, draw said box
  chatWinBox = subwin(mainWin, (LINES * 0.8), COLS, 0, 0);
  box(chatWinBox, 0, 0);
  // Draw a slick title on it
  mvwaddch(chatWinBox, 0, (COLS * 0.5) - 8, ACS_RTEE);
  wattron(chatWinBox, COLOR_PAIR(3));
  mvwaddstr(chatWinBox, 0, (COLS * 0.5) - 7, " Chat Client ");
  wattroff(chatWinBox, COLOR_PAIR(3));
  mvwaddch(chatWinBox, 0, (COLS * 0.5) + 6, ACS_LTEE);
  wrefresh(chatWinBox);
  // Create sub window in box to hold text
  chatWin = subwin(chatWinBox, (LINES * 0.8 - 2), COLS - 2, 1, 1);
  // Enable text scrolling
  scrollok(chatWin, TRUE);
}

/* Draw input box and window */
void drawInputWin()
{
  // Create input box and window
  inputWinBox = subwin(mainWin, (LINES * 0.2) - 1, COLS, (LINES * 0.8) + 1, 0);
  box(inputWinBox, 0, 0);
  mvwaddstr(inputWinBox, 0, 1, " Digite sua mensagem: ");
  inputWin = subwin(inputWinBox, (LINES * 0.2) - 3, COLS - 2, (LINES * 0.8) + 2, 1);
}

/* Receive user input from input windows */
void userInput(char *buf)
{
  // Update windows
  wrefresh(chatWin);
  wrefresh(inputWin);
  wcursyncup(inputWin);

  int i = 0;
  int ch;

  // Move cursor to correct position
  wmove(inputWin, 0, 0);
  wrefresh(inputWin);
  // Read 1 char at a time
  while ((ch = getch()) != '\n')
  {
    // Treat Left key and backspace
    if (ch == KEY_BACKSPACE || ch == KEY_LEFT)
    {
      if (i > 0)
      {
        // Erase last char
        wprintw(inputWin, "\b \b\0");
        buf[--i] = '\0';
        wrefresh(inputWin);
      }
    }
    else if (ch == KEY_RESIZE)
    {
      continue;
    }
    // Otherwise put in buffer
    else if (ch != ERR)
    {
      if (i < MEMORY_SPACE - 1)
      {
        // Append char to buffer
        strcat(buf, (char *)&ch);
        i++;
        wprintw(inputWin, (char *)&ch);
        wrefresh(inputWin);
      }
      // Unless buffer is full
      else
      {
        wprintw(inputWin, "\b%s", (char *)&ch);
        buf[(i - 1)] = '\0';
        strcat(buf, (char *)&ch);
        wrefresh(inputWin);
      }
    }
  }
  // Null terminate, clear input window
  buf[i] = '\0';
  wclear(inputWin);
  wrefresh(inputWin);
  wmove(inputWin, 0, 0);
  wrefresh(inputWin);
}

/* Handler called when window are resized */
void resizeHandler(int sig)
{
  // End current windows
  endwin();
  refresh();
  clear();

  // Redraw windows
  drawChatWin();
  drawInputWin();

  // Refresh and move cursor to input window
  wrefresh(chatWin);
  wcursyncup(inputWin);
  wrefresh(inputWin);
}

/* General function to check errors */
int checkError(int n, char *err)
{
  if (n < 0)
  {
    perror(err);
    exit(1);
  }
  return n;
}

/* Client class
  name: Name identifier of client
  connected: flag for connection
  client_socket: socket of client
  addr: address infomations
*/
class Client
{
  char name[MEMORY_SPACE / 2];
  int client_socket = -1;
  int connected = 0;
  struct sockaddr_in addr;

public:
  Client()
  {
    client_socket = -1;
    connected = 0;
  }

  /* Start client socket initial params */
  bool initClient()
  {
    if (client_socket == -1)
    {
      wprintw(chatWin, "Initing client!\n");
      wrefresh(chatWin);
      client_socket = checkError(socket(AF_INET, SOCK_STREAM, 0), (char *)"Error creating TCP socket: ");

      addr.sin_family = AF_INET;
      addr.sin_port = htons(1234);
      addr.sin_addr.s_addr = inet_addr("127.0.0.1");
      memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

      return client_socket != -1;
    }
    return false;
  }

  /* Try connection with the server */
  void connectToServer()
  {
    checkError(connect(client_socket, (struct sockaddr *)&addr, sizeof(addr)), (char *)"Error trying to connect server: ");
    connected = 1;
    wprintw(chatWin, "Conected!\n");
    wrefresh(chatWin);
  }

  /* Stop Client socket */
  void stopClient()
  {
    connected = 0;
  }

  /* Listen server messages */
  void listenServer()
  {
    int bytes_received;
    char message[MEMORY_SPACE];
    do
    {
      memset(&message, 0, sizeof(message));
      bytes_received = recv(client_socket, message, MEMORY_SPACE, MSG_DONTWAIT);
      if (bytes_received == -1)
      {
        if (errno == EWOULDBLOCK)
        {
          sleep(1);
        }
        else
        {
          perror("Error when reading message");
          exit(1);
        }
      }
      else if (bytes_received == 0)
      {
        stopClient();
      }
      else
      {
        message[bytes_received] = '\0';
        wprintw(chatWin, "%s\n", message);
        wrefresh(chatWin);
      }
    } while (connected == 1);
    wprintw(chatWin, "Servidor desconectado, basta clicar ENTER\n");
    wrefresh(chatWin);
    return;
  }

  /* Send messages to server */
  void sendServer()
  {
    int bytes_sended = -1;
    char message[MEMORY_SPACE / 2];
    do
    {
      if (bytes_sended == -1)
      {
        wprintw(chatWin, "Digite seu nome\n");
        wrefresh(chatWin);
      }
      memset(&message, 0, sizeof(message));
      userInput(message);
      if (connected == 1)
      {
        bytes_sended = send(client_socket, message, strlen(message), 0);
        if (strcmp(message, "exit") == 0)
        {
          stopClient();
        }
      }
    } while (connected == 1);
    close(client_socket);
    resetClient();
    return;
  }

private:
  /* Reset Client */
  void resetClient()
  {
    client_socket = -1;
    connected = 0;
  }
};

Client client;

/* Func to spam listen thread */
void listenStart()
{
  client.listenServer();
  return;
}

/* Func to spam send thread */
void sendStar()
{
  client.sendServer();
  return;
}

int main()
{
  signal(SIGWINCH, resizeHandler);

  mainWin = initscr();
  noecho();
  cbreak();
  keypad(mainWin, TRUE);
  colors();

  drawChatWin();
  drawInputWin();

  if (!client.initClient())
  {
    return 0;
  }

  client.connectToServer();

  thread th1(listenStart);
  thread th2(sendStar);

  th1.join();
  th2.join();

  wrefresh(chatWin);
  endwin();

  return 0;
}