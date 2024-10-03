A POC testbed is set up to demo how a K8s pod opens and processes a TCP/IP socket within another pod on the same node using the Linux namespace API.

![Kubernetes inter-pod socketing](Namespace_socket_poc.png)

In this example, the backend pod runs as a popen(3) server that receives shell commands from a client pod, execute them locally and return the results to the client. Instead of the backend pod, the server process creates a socket in the frontend pod to listen and accept popen requests from the client pod. Consequently, the client pod connects to the frontend to send popen requests, which are ready to be picked up and processed by the backend server.

### Building the popen(3) server

Running on the backend node, the server is designed to perform the following key tasks.

1. Call system(3) to use the cictl CLI to retrieve the container ID and process ID <pid> the front pod.
2. Open /proc/<pid>/ns/net that represents the Linux network namespace of the front end container process.
3. Call setns(3)
