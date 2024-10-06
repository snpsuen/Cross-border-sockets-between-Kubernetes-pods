A POC testbed is set up to demo how a K8s pod opens and processes a TCP/IP socket within another pod on the same node using the Linux namespace API.

![Kubernetes inter-pod socketing](Namespace_socket_poc02.png)

In this example, the backend pod runs as a popen(3) server that receives shell commands from a client pod, execute them locally and return the results to the client. Instead of the backend pod, the server process creates a socket in the frontend pod to listen and accept popen requests from the client pod. Consequently, the client pod connects to the frontend to send popen requests, which are ready to be picked up and processed by the backend server.

### Build a Linux namespace-aware popen(3) server

Running on a backend pod, the server is designed to perform the following key tasks.

1. Call system(3) to use the crictl CLI to retrieve the container ID and process ID $pid the front pod.
2. Open /proc/$pid/ns/net that represents the Linux network namespace of the front end container process.
3. Call setns(3) to set the Linux network namespace of the server temporarily to that of the front pod.
4. Call socket() to create a socket listening on the front pod. From now on, any worker sockets springing up to accept client requests from the listening socket will likewise reside in the front pod.
5. Call setns(3) to return to the original Linux network namespace of the server.
6. Go through the standard TCP concurrent server workflow with the sockets to handle each client request with popen(3) in a child process.

Build the server program from the source code in source/popen_server_ns.c.
```
gcc -o pop_server_ns popen_server_ns.c
```

### Deploy a backend pod 

The docker image of the pod is based on a well known swiss army knife style container for network troubleshooting, https://github.com/nicolaka/netshoot/.
More specifically, the binaries of the namespacepopen(3) server and crictl are copied to the docker image.
```
cat > Dockerfile <<END
FROM nicolaka/netshoot
COPY ./crictl /bin
COPY ./popen_server_ns /bin
USER root
CMD ["sleep", "infinity"]
END

docker build -t snpsuen/backend_popen:v2 -f Dockerfile .
```
The backend docker is then pulled to run as a kubernetes pod customised with two additional properties.
*  set hostPID to true to allow the node to shared its entire process tree with the pod.
*  mount a host path of /run to provide a runtime container endpoint for crictl.
```
kubectl run backender --image=snpsuen/backend_popen:v2 \
--overrides '
{
  "spec": {
    "containers": [
	  {
	    "image": "snpsuen/backend_popen:v2",
        "name": "backender",
	    "securityContext": {"privileged": true},
		"volumeMounts": [
          {
            "mountPath": "/run",
            "name": "run-volume"
          }
        ]
      }
    ],
    "hostPID": true,
	 "volumes": [
      {
        "name": "run-volume",
        "hostPath": {
          "path": "/run"
        }
      }
    ]
  }
}' -- sleep "infinity"
```
### Deploy a frontend pod 

Use kubectl to run a frontend pod on the fly on Kubernetes based on another swiss army knife docker sample, https://github.com/leodotcloud/swiss-army-knife.

```
kubectl run frontender --image=leodotcloud/swiss-army-knife -- sleep infinity
```
### Deploy a client pod 

Compile and link the popen(3) client program from the source in source/popen_client.c.
```
gcc -o pop_server_ns popen_client.c
```
Create a docker image for the client pod that by copying the binaries of the client program to the busybox image, https://hub.docker.com/_/busybox.
```
cat > Dockerfile <<END
FROM busybox:latest
COPY ./popen_client /bin
CMD ["sleep", "infinity"]
END

docker build -t snpsuen/busypopenclient:v1 -f Dockerfile .
```
Spin up the client pod by running it directly on the newly built docker image on kubernetes.
```
kubectl run busyclient2 --image=snpsuen/busypopenclient:v2
```
### Try out the whole thing

Deploy the backend, frontend and client pods on Kubernetes.
```
keyuser@ubunclone:~$ kubectl run backender --image=snpsuen/backend_popen:v2 \
--overrides '
{
  "spec": {
    "containers": [
          {
            "image": "snpsuen/backend_popen:v2",
        "name": "backender",
            "securityContext": {"privileged": true},
                "volumeMounts": [
          {
            "mountPath": "/run",
            "name": "run-volume"
          }
        ]
      }
    ],
    "hostPID": true,
         "volumes": [
      {
        "name": "run-volume",
        "hostPath": {
          "path": "/run"
        }
      }
    ]
  }
}'
pod/backender created
keyuser@ubunclone:~$ kubectl run frontender --image=leodotcloud/swiss-army-knife -- sleep infinity
pod/frontender created
keyuser@ubunclone:~$ kubectl run busyclient2 --image=snpsuen/busypopenclient:v2
pod/busyclient2 created
```
Observe that the backend and frontend pods are running on the same K8s node, namely ambient-worker2.
```
keyuser@ubunclone:~$ kubectl get pods -o wide
NAME          READY   STATUS    RESTARTS   AGE     IP           NODE              NOMINATED NODE   READINESS GATES
backender     1/1     Running   0          3m46s   10.244.1.3   ambient-worker2   <none>           <none>
busyclient2   1/1     Running   0          2m30s   10.244.1.5   ambient-worker2   <none>           <none>
frontender    1/1     Running   0          2m56s   10.244.1.4   ambient-worker2   <none>           <none>
keyuser@ubunclone:~$
```
Execute the popen server on the backend pod, with the frontend pod set to frontender. Note the TCP port number of the server is set arbitarily to 42855.
```
keyuser@ubunclone:~$ kubectl exec -it backender -- bash
backender:~# popen_server_ns frontender &
[1] 1536
backender:~# WARN[0000] runtime connect using default endpoints: [unix:///run/containerd/containerd.sock unix:///run/crio/crio.sock unix:///var/run/cri-dockerd.sock]. As the default settings are now deprecated, you should set the endpoint instead.
WARN[0000] image connect using default endpoints: [unix:///run/containerd/containerd.sock unix:///run/crio/crio.sock unix:///var/run/cri-dockerd.sock]. As the default settings are now deprecated, you should set the endpoint instead.
WARN[0000] runtime connect using default endpoints: [unix:///run/containerd/containerd.sock unix:///run/crio/crio.sock unix:///var/run/cri-dockerd.sock]. As the default settings are now deprecated, you should set the endpoint instead.
Server is listening to port no. 42855
backender:~#
```
Execute the popen client on the client pod by setting the service endpoint to the frontend pod and port number to 42855.
```
keyuser@ubunclone:~$ kubectl exec -it busyclient2 -- sh
/ #
/ # popen_client 10.244.1.4 42855
Enter remote command or "quit":>>
```




