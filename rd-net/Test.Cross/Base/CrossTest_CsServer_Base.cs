using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using JetBrains.Collections.Viewable;
using JetBrains.Rd;
using JetBrains.Rd.Impl;
using Test.RdCross.Util;

namespace Test.RdCross.Base
{
  // ReSharper disable once InconsistentNaming
  public abstract class CrossTest_CsServer_Base : CrossTest_Cs_Base
  {
    protected readonly int Port;

    static int FindFreePort()
    {
      TcpListener l = new TcpListener(IPAddress.Loopback, 0);
      l.Start();
      int port = ((IPEndPoint) l.LocalEndpoint).Port;
      l.Stop();
      return port;
    }

    protected CrossTest_CsServer_Base()
    {
      Port = FindFreePort();

      using (var stream = new StreamWriter(File.OpenWrite(FileSystem.PortFile)))
      {
        stream.WriteLine(Port);
      }

      Console.WriteLine($"port={Port} 's written in file=${FileSystem.PortFile}");
      
      using (File.Create(FileSystem.PortFileStamp)) { }
    }

    protected void Queue(Action action)
    {
      SingleThreadScheduler.RunOnSeparateThread(SocketLifetime, "Worker", scheduler =>
      {
        var client = new SocketWire.Server(ModelLifetime, scheduler, new IPEndPoint(IPAddress.Loopback, Port),
          "DemoServer");
        var serializers = new Serializers();
        Protocol = new Protocol("Server", serializers, new Identities(IdKind.Server), scheduler, client,
          SocketLifetime);
        scheduler.Queue(action);
      });
    }
  }
}