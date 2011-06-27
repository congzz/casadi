// lqrTest.cpp

#include <iostream>
#include <cstdlib>

#include <casadi/stl_vector_tools.hpp>
#include <casadi/sx/sx_tools.hpp>
#include <casadi/fx/sx_function.hpp>
//#include <casadi/fx/jacobian.hpp>

#include "Ode.hpp"
#include "Ocp.hpp"
#include "MultipleShooting.hpp"
#include "Lqr.hpp"

#include <SnoptInterface.hpp>

#include <string>
#include <map>

using namespace CasADi;
using namespace std;

static SnoptInterface solve_cartpole(void);


void
dxdt(map<string,SX> &xDot, map<string,SX> &outputs, map<string,SX> state, map<string,SX> action, map<string,SX> param, SX t)
{
	// constants
	double g = 9.8;    // acceleration due to gravity
	double l = 2.2;
	double mc = 10;
	double mp = 5;
	

	SX x = state["x"];
	SX theta = state["theta"];
	SX vx = state["vx"];
	SX vtheta = state["vtheta"];
	SX u = action["u"];

	SX ax = 1/(mc+mp*sin(theta)*sin(theta))*(u+mp*sin(theta)*(l*vtheta*vtheta+g*cos(theta)));
	SX atheta = 1/(l*(mc+mp*sin(theta)*sin(theta)))*(-u*cos(theta) - mp*l*vtheta*vtheta*cos(theta)*sin(theta) - (mc+mp)*g*sin(theta));

	xDot["x"] = vx;
	xDot["theta"] = vtheta;
	xDot["vx"] = ax;
	xDot["vtheta"] = atheta;
}

Ode
getOde()
{
	Ode ode("cartpole");
	ode.addState("x");
	ode.addState("theta");
	ode.addState("vx");
	ode.addState("vtheta");

	ode.addAction("u");
	ode.addAction("uDummy");

	ode.dxdt = &dxdt;

	return ode;
}

SX
cost(map<string,SX> state, map<string,SX> action)
{

#define SQR(sqr_me) ((sqr_me)*(sqr_me))

	SX cost = 2*SQR(state["x"]) + 3*SQR(state["theta"]) + 4*SQR(state["vx"]) + 5*SQR(state["vtheta"]) + 6*SQR(action["u"]);
	cost += 7*state["x"]*action["u"];

	cost += SQR(action["uDummy"]);

	return cost;
}


int
main()
{
	// SnoptInterface si = solve_cartpole();
	// si.run();
	// vector<double>xopt(ms.getBigN());
	// solver.getOutput(xopt,NLP_X_OPT);
	// ocp.writeOctaveOutput("cartpole_out", si.x);
	// si.writeOctaveOutput("multipliers_out");
	
	// // Print the optimal cost
	// cout << "optimal time: " << si.F[0] << endl;
	//cout << "optimal solution: " << xopt << endl;
	
	
	Ode ode = getOde();
	double t0 = 0;
	double tf = 6;
	int N = 2;
	Lqr lqr(ode, t0, tf, N, &cost);

	lqr.runBackwardSweep();
	
	cout << "successful finish\n";
	return 0;
}





SnoptInterface
solve_cartpole()
{
  double trackLength = 4;
  
  Ode ode = getOde();
  Ocp ocp;
  SX tEnd = ocp.addParam("tEnd");
  
  MultipleShooting & ms = ocp.addMultipleShooting("cartpole", ode, 0.0, tEnd, 60);

	int N = ms.N;
  
  // cost function
  SX xf = ms.getState("x", ms.N-1);
  SX thetaf = ms.getState("theta", N-1);
  SX vthetaf = ms.getState("vtheta", N-1);

  
  ocp.objFun = tEnd+50*cos(thetaf)+5*(vthetaf)*vthetaf; // minimum time
  //ocp.objFun = tEnd;
  
  
  // bounds
  ocp.boundParam("tEnd", 4, 50);
	
  ms.boundStateAction("x", -trackLength/2, trackLength/2);
  ms.boundStateAction("vx", -22, 22);
  ms.boundStateAction("theta", -50, 50);
  ms.boundStateAction("vtheta", -50, 50);

  ms.boundStateAction("u",-20,20);

  /// initial conditions
  ms.boundStateAction("x",0,0,0);
  ms.boundStateAction("theta",0.1,0.1,0);
  ms.boundStateAction("vx",0,0,0);
  ms.boundStateAction("vtheta",0,0,0);

	ocp.addNonlconIneq(ms.getState("x",0), "startx");
	ocp.addNonlconEq(ms.getState("vx",N/2), "xstall");	

//   for(int k = 0; k < ms.N; k++) {
//     ms.setStateActionGuess("theta", double(k)*3.1415/ms.N, k);
//     ms.setStateActionGuess("vtheta", 3.1415/ms.N, k);
//   }

//   ms.boundStateAction("theta",3.1415,3.1415,ms.N-1);
//   ms.boundStateAction("x",0, 0, ms.N-1);
//   ms.boundStateAction("vx",0, 0, ms.N-1);
//   ms.boundStateAction("vtheta",0, 0, ms.N-1);




  // solve
  SnoptInterface si(ocp);

  return si;
}
