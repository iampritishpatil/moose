// moose
// genesis


echo "
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Much like the axon demo, a linear excitable cell is created using readcell.   %
% Integration is done using the Hines' method.                                  %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
"

////////////////////////////////////////////////////////////////////////////////
// COMPATIBILITY (between MOOSE and GENESIS)
////////////////////////////////////////////////////////////////////////////////
include compatibility.g


////////////////////////////////////////////////////////////////////////////////
// MODEL CONSTRUCTION
////////////////////////////////////////////////////////////////////////////////
float SIMDT = 50e-6
float IODT = 100e-6
float SIMLENGTH = 0.25
float INJECT = 1e-10
float EREST_ACT = -0.065

include chan.g

ce /library
	make_Na_mit_usb
	make_K_mit_usb
ce /

//=====================================
//  Create cells
//=====================================
readcell myelin2.p /axon


////////////////////////////////////////////////////////////////////////////////
// PLOTTING
////////////////////////////////////////////////////////////////////////////////
create neutral /plot

create table /plot/Vm0
call /plot/Vm0 TABCREATE {SIMLENGTH / IODT} 0 {SIMLENGTH}
setfield /plot/Vm0 step_mode 3
addmsg /axon/soma /plot/Vm0 INPUT Vm

create table /plot/Vm1
call /plot/Vm1 TABCREATE {SIMLENGTH / IODT} 0 {SIMLENGTH}
setfield /plot/Vm1 step_mode 3
addmsg /axon/n99/i20 /plot/Vm1 INPUT Vm


////////////////////////////////////////////////////////////////////////////////
// SIMULATION CONTROL
////////////////////////////////////////////////////////////////////////////////

//=====================================
//  Clocks
//=====================================
if ( MOOSE )
	setclock 0 {SIMDT} 0
	setclock 1 {SIMDT} 1
	setclock 2 {IODT} 0
else
	setclock 0 {SIMDT}
	setclock 1 {SIMDT}
	setclock 2 {IODT}
end

useclock /plot/Vm0 2
useclock /plot/Vm1 2

//=====================================
//  Stimulus
//=====================================
setfield /axon/soma inject {INJECT}

//=====================================
//  Solvers
//=====================================
if ( GENESIS )
	create hsolve /axon/solve
	setfield /axon/solve \
		path /axon/##[TYPE=symcompartment],/axon/##[TYPE=compartment] \
		comptmode 1  \
		chanmode 3
	call /axon/solve SETUP
	setmethod 11
end

//=====================================
//  Simulation
//=====================================
reset
step {SIMLENGTH} -time


////////////////////////////////////////////////////////////////////////////////
//  Write Plots
////////////////////////////////////////////////////////////////////////////////
openfile "axon.0.plot" w
writefile "axon.0.plot" "/newplot"
writefile "axon.0.plot" "/plotname Vm(0)"
closefile "axon.0.plot"
tab2file axon.0.plot /plot/Vm0 table

openfile "axon.x.plot" w
writefile "axon.x.plot" "/newplot"
writefile "axon.x.plot" "/plotname Vm(100)"
closefile "axon.x.plot"
tab2file axon.x.plot /plot/Vm1 table

echo "
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Plots written to axon.*.plot.                                                   %
% If you have gnuplot, run 'gnuplot myelin.gnuplot' to view the graphs.           %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
"
quit
