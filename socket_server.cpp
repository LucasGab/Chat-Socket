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

#define MEMORY_SPACE 1024

using namespace std;

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
  socket: socket of client
  position: position of client in the representation array
*/
class Client
{
public:
  char name[MEMORY_SPACE];
  int socket;
  int connected;
  int position;

  /* Close client socket */
  void closeClient()
  {
    if (this->isClientActive())
    {
      close(socket);
    }
    resetClient();
  }

  /* Check if client is active */
  bool isClientActive()
  {
    return (socket != -1 && (connected == 1 || position != -1));
  }

  /* Reset client params */
  void resetClient()
  {
    connected = 0;
    position = -1;
    socket = -1;
    memset(&name, 0, sizeof(name));
  }
};

/* ChatServer class
  MAX_CLIENTS: Max number of clients concurrent on server
  server_socket: socket of server
  server_status: flag to indicate server status
  clients_socket: array of clients in server
  addr: Address information about server
  next_position: Stack to control postion to alocate new clients
  clients_threads: Array of spawned thread of clients listeners
*/

class ChatServer
{

  static int const MAX_CLIENTS = 10;
  int server_socket = -1;
  int server_status = 0;
  Client clients_socket[MAX_CLIENTS];
  struct sockaddr_in addr;
  stack<int> next_position;
  thread clients_threads[MAX_CLIENTS];

public:
  ChatServer()
  {
    server_socket = -1;
    server_status = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      clients_socket[i].resetClient();
      next_position.push(i);
    }
  }

  /* Start initial server socket params */
  bool initServer()
  {
    if (server_socket == -1)
    {
      printf("Initing server\n");
      server_socket = checkError(socket(AF_INET, SOCK_STREAM, 0), (char *)"Error creating TCP socket: ");
      int flags = checkError(fcntl(server_socket, F_GETFL), (char *)"Error getting flags TCP socket: ");
      checkError(fcntl(server_socket, F_SETFL, flags | O_NONBLOCK), (char *)"Error setting flags TCP socket: ");

      addr.sin_family = AF_INET;
      addr.sin_port = htons(1234);
      addr.sin_addr.s_addr = INADDR_ANY; //inet_addr("127.0.0.1");
      memset(&addr.sin_zero, 0, sizeof(addr.sin_zero));

      checkError(bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)), (char *)"Error trying to bind socket: ");
      checkError(listen(server_socket, MAX_CLIENTS), (char *)"Error trying to listen socket: ");

      server_status = 1;
      return server_socket != -1;
    }
    return false;
  }

  /* Start listening to clients connections */
  void listenClients()
  {
    printf("Start to listen clients\n");
    receiveClients();
    return;
  }

  /* Stop server */
  void stopServer()
  {
    server_status = 0;
  }

  /* Check if server is actived */
  bool isServerActive()
  {
    return server_status == 1;
  }

private:
  /* Reset server params */
  void resetServer()
  {
    if (isServerActive())
    {
      close(server_socket);
    }
    server_socket = -1;
    server_status = 0;
  }

  /* Send message to all active clients */
  void sendToAllSubscribers(char *message)
  {
    printf("%s\n", message);
    int bytes_sended;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients_socket[i].socket >= 0 && clients_socket[i].connected == 1)
      {
        bytes_sended = send(clients_socket[i].socket, message, strlen(message), 0);
      }
    }
  }

  /* Read and process clients message to foward to clients */
  int processMessage(Client client, char message[MEMORY_SPACE], int bytes_received, int message_type)
  {
    if (strcmp(message, "exit") == 0)
    {
      return 0;
    }

    if (message_type == 0)
    {
      char pre[] = " entrou!\0";
      strcat(message, pre);
      sendToAllSubscribers(message);
    }
    else
    {
      char message_final[2 * MEMORY_SPACE];
      strncpy(message_final, client.name, MEMORY_SPACE);
      message_final[strlen(message_final)] = ':';
      strcat(message_final, message);
      message_final[strlen(message_final)] = '\0';
      sendToAllSubscribers(message_final);
    }
    return 1;
  }

  /* Receive client message */
  void receiveMessage(int position)
  {
    Client client = clients_socket[position];
    int message_number = 0;
    int bytes_received, enviados;
    char mensagem[MEMORY_SPACE];
    char message_client[MEMORY_SPACE];
    int status = 1;
    
    do
    {
      bytes_received = recv(client.socket, message_client, MEMORY_SPACE, 0);
      if (bytes_received == -1)
      {
        if (errno == EWOULDBLOCK)
        {
          sleep(1);
        }
        else
        {
          perror("Error when accepting message");
          exit(1);
        }
      }
      else
      {
        message_client[bytes_received] = '\0';
        int message_type = 1;
        if (message_number == 0)
        {
          strncpy(client.name, message_client, MEMORY_SPACE);
          client.connected = 1;
          message_type = 0;
        }

        status = processMessage(client, message_client, bytes_received, message_type);

        message_number++;
      }
    } while (status == 1 && isServerActive());
    char message_final[2 * MEMORY_SPACE];
    strncpy(message_final, client.name, MEMORY_SPACE);
    strcat(message_final, " saiu!");
    sendToAllSubscribers(message_final);
    client.closeClient();
    next_position.push(position);
    return;
  }

  /* Receive client and spawn thread for listen this client */
  void receiveClients()
  {
    int message_flag = 0;
    while (isServerActive())
    {
      if (message_flag == 0)
      {
        printf("Waiting new clients\n");
        message_flag = 1;
      }
      int socket_client = accept(server_socket, 0, 0);

      if (socket_client == -1)
      {
        if (errno == EWOULDBLOCK)
        {
          sleep(1);
        }
        else
        {
          perror("Error when accepting connection");
          exit(1);
        }
      }
      else
      {
        printf("A new client entered\n");
        Client client;
        int position = next_position.top();
        int flags = checkError(fcntl(socket_client, F_GETFL), (char *)"Error getting client flags TCP socket: ");
        checkError(fcntl(socket_client, F_SETFL, flags | O_NONBLOCK), (char *)"Error setting client flags TCP socket: ");
        client.socket = socket_client;
        client.position = position;
        client.connected = 1;
        clients_socket[position] = client;
        next_position.pop();
        message_flag = 0;
        if (clients_threads[position].joinable()) {
          clients_threads[position].join();
        }
        clients_threads[position] = thread(&ChatServer::receiveMessage, this, ref(position));
      }

      while (next_position.empty())
      {
        if (message_flag == 0)
        {
          printf("Server is full!\n");
          message_flag = 2;
        }
      }
      if (message_flag == 2)
      {
        message_flag = 0;
      }
    }
    closeAllClients();
    close(server_socket);
    resetServer();
    return;
  }

  /* Close all clients connections and threads */
  void closeAllClients()
  {
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients_socket[i].isClientActive())
      {
        close(clients_socket[i].socket);
        clients_threads[i].join();
      }
    }
  }
};

ChatServer server;

/* Func to spawn listen server */
void listenServer()
{
  server.listenClients();
}

int main()
{

  if (!server.initServer())
  {
    return 0;
  }

  thread th1(listenServer);
  //char command[256];
  string command;
  do
  {
    printf("Server: ");
    cin >> command;
  } while (command.compare("exit") != 0);

  cout << "Saindo" << endl;
  server.stopServer();
  th1.join();
  return 0;
}