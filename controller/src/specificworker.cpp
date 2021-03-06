/*
 *    Copyright (C) 2015 by YOUR NAME HERE
 *
 *    This file is part of RoboComp
 *
 *    RoboComp is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RoboComp is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with RoboComp.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "specificworker.h"
#include </home/ivan/robocomp/components/g1/GoToPoint/src/specificworker.h>
#include <qt4/Qt/qlocale.h>
#include <qt4/Qt/qvarlengtharray.h>
static float sentido=M_PI;
/**
* \brief Default constructor
*/
SpecificWorker::SpecificWorker(MapPrx& mprx) : GenericWorker(mprx)
{
	connect(Start, SIGNAL(clicked()), this, SLOT(iniciar()));
	connect(Stop, SIGNAL(clicked()), this, SLOT(parar()));
	connect(&clk, SIGNAL(senal()), this, SLOT(reloj()));
	
	clk.setseg(1000);
	scene =  new QGraphicsScene();
	Grafico->setScene(scene);
	startbutton=false;
	clk.start();
	st=State::INIT;
	cota = 16;
	distsecurity = 440;
// 	inner=NULL;
	inner = new InnerModel("/home/ivan/robocomp/files/innermodel/RoCKIn@home/world/apartment.xml");
// 	inner = new InnerModel("/home/ivan/robocomp/files/innermodel/simpleworld.xml");
	map = new MapQVec(Grafo);
	cost = new LengthMap(Grafo);
	
	differentialrobot_proxy->getBaseState(state);
	QVec a=QVec::vec3(state.x,0,state.z);
	robotnode = Grafo.addNode();
	(*map)[robotnode]=a;
	pintarnodo( a, a);
	ldata = laser_proxy->getLaserData();
	muestreolaser=(ldata[51].angle-ldata[50].angle);
	mldata=ldata.size()/2;
	pick=QVec::vec3(0,0,0);
	circulo=NULL;
}

/**
* \brief Default destructor
*/
SpecificWorker::~SpecificWorker(){  
}

bool SpecificWorker::setParams(RoboCompCommonBehavior::ParameterList params)
{
	timer.start(Period);
// 	try
// 	{
// 		RoboCompCommonBehavior::Parameter par = params.at("InnerModelPath");
// 		string innermodel_path=par.value;
// 		try
// 		{	
// 			inner = new InnerModel(innermodel_path);
// 		}
// 		catch (QString &s)
// 		{
// 			printf("error reading iner model %s\n", s.toStdString().c_str());
// 		}
// 		
// 	}
// 	catch(std::exception e) { qFatal("Error reading config params"); }
	return true;
}
void SpecificWorker::compute()
{
	try
	{
		pintarrobot();
		ldata = laser_proxy->getLaserData();
		ldatacota = laser_proxy->getLaserData();
		ldatasinord = laser_proxy->getLaserData();
		std::sort( ldata.begin(), ldata.end(), [](RoboCompLaser::TData a, RoboCompLaser::TData b){ return (a.dist < b.dist); }) ;  //sort laser data from small to $
		std::sort( ldatacota.begin()+cota, ldatacota.end()-cota, [](RoboCompLaser::TData a, RoboCompLaser::TData b){ return (a.dist < b.dist); }) ;  //sort laser data from small to $
		differentialrobot_proxy->getBaseState(state);
		inner->updateTransformValues("robot",state.x,0,state.z,0,state.alpha,0);
		NavState s=controller_proxy->getState();
		pintarposerobot();
		switch(st)
		{
			case State::INIT:
			  break;
			case State::MAPEAR:
				if(esquina())
				{
					accionEsquina();
				}
				else
				{
					st=mapear();
				}
			  break;
			case State::HELLEGADO:
					st=hellegado();
			  break;
			case State::GOTOPOINTS:
					st=goto_point();
			  break;
			case State::CHECKPOINT:
					st= checkpoint();
			  break;
		}
	}
	catch(const Ice::Exception &ex)
	{
		  std::cout << ex << std::endl;
	}	
}
SpecificWorker::State SpecificWorker::mapear()
{
  try
  {
		borrarcirculo();
		if(pick==QVec::vec3(0,0,0)){
			this->n = QVec::zeros(3);
			this->n[0] = QVec::uniformVector(1, xmin->text().toInt(), xmax->text().toInt())[0];
			this->n[2] = QVec::uniformVector(1, ymin->text().toInt(), ymax->text().toInt())[0];
		}
		else{
			n=pick;
			pick=QVec::vec3(0,0,0);
		}
			

		n[1] = 0;
		pintardestino(n);
		writeinfo("Point = ("+to_string(n(0))+","+to_string(n(2))+")");	

		qDebug() << __FUNCTION__ << n;
		Node nodoDestino;
		float dist = std::numeric_limits< float >::max(), d;		
		for (lemon::ListGraph::NodeIt _n(Grafo); _n != lemon::INVALID; ++_n)
		{
			d = (n - map->operator[](_n)).norm2();
	
			if( d < dist ) 
			{ 
				dist = d;
				nodoDestino = _n;
			}	
		}
		qDebug() << __FUNCTION__ << "dist" << d << "node" << map->operator[](nodoDestino);
// 		if((map->operator[](nodoDestino)-n).norm2()>700)return State::MAPEAR; //variante del algoritmo RRT
		if( nodoDestino != robotnode)
		{
			dijkstra(nodoDestino);
			qDebug() << "---------------";
		}	
		else
		  qDebug() << "-----ya esta en el nodo-----";
	}
	catch(const Ice::Exception &ex)
  {
        std::cout << ex << std::endl;
  }
  return State::GOTOPOINTS;
}
SpecificWorker::State SpecificWorker::checkpoint()
{	
	qDebug() << __FUNCTION__<<"---inicio";
	QVec qposR = inner->transform("laser",n,"world");
	float alfa = atan2(qposR.x(), qposR.z());
	if(fabs(alfa)>muestreolaser){
		differentialrobot_proxy->setSpeedBase(0,alfa*2);
		usleep(500000);
		differentialrobot_proxy->setSpeedBase(0,0);
	}
	float dist = qposR.norm2();
	qDebug()<< ldatasinord[mldata].dist<< "<"<< dist;
	if( ldatasinord[mldata].dist < dist-ROBOT_SIZE)
	{
		qDebug()<<"Entro";
		n = inner->laserTo("world","laser",ldatasinord[mldata].dist-ROBOT_SIZE,ldatasinord[mldata].angle);
		return State::CHECKPOINT;
	}
	
	RoboCompController::TargetPose t;
	t.x=n(0);
	t.z=n(2);
	t.y=0;
	qDebug()<<"goto----"<<n;
	controller_proxy->go(t);
	return State::HELLEGADO;
	
}
SpecificWorker::State SpecificWorker::goto_point()
{
	NavState s=controller_proxy->getState();
	if(pila.isEmpty()&&(s.state=="FINISH"||s.state=="IDLE"))
		return State::CHECKPOINT;
	if(s.state=="FINISH"||s.state=="IDLE"){
		RoboCompController::TargetPose proximo;
		Node siguiente;
		QVec d;
		siguiente=pila.pop();
		d=map->operator[](siguiente);
		proximo.x=d(0);
		proximo.z=d(2);
		proximo.y=0;
		qDebug()<<"goto----"<<d;
		controller_proxy->go(proximo);
		anterior =map->operator[](robotnode);
		robotnode=siguiente;
	}
	return State::GOTOPOINTS;
}
void SpecificWorker::iralpuntomascercano() //Calculo el nodo origen del grafo y el nodo destino del grafo.
{
	Node nodedestino;
	float dismin=std::numeric_limits<float>::max();
	//calculo el node destino
	for(NodeIt aux(Grafo); aux!=INVALID;++aux){
		float d=((*map)[aux]-n).norm2();
		if(dismin>d){
			dismin=d;
			nodedestino=aux;
		}
	}
	if(nodedestino!=robotnode){
		dijkstra(nodedestino);
		st=State::GOTOPOINTS;  
	}
	else
		st = State::CHECKPOINT;
}
SpecificWorker::State SpecificWorker::hellegado()
{
	qDebug() << __FUNCTION__<<"---inicio";
	NavState s=controller_proxy->getState();
	cout << s.state<<endl;
	if(s.state=="FINISH"||s.state=="IDLE"){
		
		cout << s.SubPoints <<endl;
		
		char *puntos = new char[s.SubPoints.length() + 1];
		strcpy(puntos, s.SubPoints.c_str());
		char * pch;
		pch = strtok (puntos,"/");
		QVec Sub=QVec::vec3(0,0,0);
		while (pch != NULL)
		{	
			printf ("%s\n",pch);
			char *pEnd;
			Sub(0)=strtof(pch,&pEnd);
			Sub(2)=strtof(pEnd,NULL);
			
			//add new node to the graph
			lemon::ListGraph::Node newP = Grafo.addNode();
			map->set(newP, Sub);
			lemon::ListGraph::Edge newEdge = Grafo.addEdge(robotnode,newP);
			cost->set( newEdge, (map->operator[](newP) - map->operator[](robotnode)).norm2());
			
			pintarnodo(map->operator[](robotnode),map->operator[](newP));
			robotnode=newP;
			pch = strtok (NULL, "/");
			
			
		}
		qDebug() << "Listing the graph";
		for (lemon::ListGraph::NodeIt n(Grafo); n != lemon::INVALID; ++n)
			qDebug() << map->operator[](n);
		qDebug() << "---------------";
		qDebug() << __FUNCTION__<<"---fin";
		return State::MAPEAR;
	}
	qDebug() << __FUNCTION__<<"---fin";
	return State::HELLEGADO;
}
bool SpecificWorker::puntodentrocampolaser(int &pos,float angle, int distpoint)
{
	pos=-1;
	float angmin=ldatasinord[0].angle-angle;
	if(fabs(angle)<=M_PI/2)
	{
		for(int i=0; i<(int)ldatasinord.size();i++){
			if(fabs(ldatasinord[i].angle-angle)<angmin){
				angmin=fabs(ldatasinord[i].angle-angle);
				pos=i;
			}
		}
	}
	if (pos==-1)
	  return false;
	else 
	  if(ldatasinord[pos].dist<distpoint)
		  return false;
	  else 
		  return true;
	
}
void SpecificWorker::anadirnodo(QVec nuevo)
{
	robotnode = Grafo.addNode();	
	(*map)[robotnode]=nuevo;
	for(NodeIt n(Grafo); n!=INVALID;++n)
	{
		if((*map)[n]==anterior)
		{
			Edge a=Grafo.addEdge(n,robotnode);
			(*cost)[a]=((*map)[n]-(*map)[robotnode]).norm2();
			break;
		}
	}
}
void SpecificWorker::dijkstra(Node destino)
{
	qDebug() << __FUNCTION__<<"----inicio";
	Dijkstra<Graph,LengthMap> distra(Grafo, (*cost));
	if(distra.run(robotnode,destino)){
		
		Node n=destino;
		pila.push(n);
		qDebug()<<"Pila";
		qDebug()<<(*map)[n];
		while ((n = distra.predNode(n))!=lemon::INVALID) {
			pila.push(n);
			qDebug()<<(*map)[n];
		}
	}
	qDebug() << __FUNCTION__<<"----fin" ;
}
bool SpecificWorker::esquina()
{
	RoboCompLaser::TLaserData ldataaux = laser_proxy->getLaserData();
	return (ldataaux.data()+cota)->dist<distsecurity&&((ldataaux.data())+100-cota)->dist<distsecurity;
}
void SpecificWorker::accionEsquina()
{
	writeinfo("Esquina");
	differentialrobot_proxy->setSpeedBase(-400, 0); 				// DA MARCHA ATRAS
	usleep(1000000);
	RoboCompLaser::TLaserData ldataaux = laser_proxy->getLaserData();  //read laser data
	if(ldataaux.data()->dist<(ldataaux.data()+99)->dist)
		sentido=-sentido;
	differentialrobot_proxy->setSpeedBase(0, sentido);			//GIRA SOBRE SÍ MISMO
	usleep(1000000);
	differentialrobot_proxy->setSpeedBase(0, 0);
}
void SpecificWorker::setPick(const Pick& myPick)
{
	pick(0) = myPick.x;
	pick(1) = myPick.y;
	pick(2) = -myPick.z;
	qDebug()<< __FUNCTION__ << myPick.x << myPick.y << -myPick.z;
}
void SpecificWorker::reloj()
{
	static int seg=1;
	static int min=1;
	static int hor=1;
	if(startbutton){
		if(seg<60){
			LCDseg->display(seg++);
		}
		else if(seg==60&&min<60){
			seg=1;
			LCDseg->display(seg);
			LCDmin->display(min++);
		}
		else {
			seg=1;
			min=1;
			LCDseg->display(seg);
			LCDmin->display(min);
			LCDhor->display(hor++);
		}
	}
}
void SpecificWorker::iniciar()
{
	st=State::MAPEAR;
	startbutton=true;
}
void SpecificWorker::parar()
{
	try{
	startbutton=false;
	st=State::INIT;
	while (!pila.isEmpty())
		pila.pop();
	borrarcirculo();
	differentialrobot_proxy->setSpeedBase(0, 0);
	controller_proxy->stop();
	}
	catch(const Ice::Exception &ex)
	{
		  std::cout << ex << std::endl;
	}	
}
void SpecificWorker::writeinfo(string _info)
{
	QString *text=new QString(_info.c_str());
	InfoText->append(*text);
} 
void SpecificWorker::pintarnodo(QVec origen, QVec destino)
{
	scene->addLine(origen(0)/10, -origen(2)/10, destino(0)/10, -destino(2)/10, QPen(Qt::black));
	scene->addEllipse(destino(0)/10-5, -destino(2)/10-5, 10, 10, QPen(Qt::white), QBrush(Qt::black));
}
void SpecificWorker::pintardestino(QVec destino)
{
	circulo = scene->addEllipse(destino(0)/10-5, -destino(2)/10-5, 10, 10, QPen(Qt::white), QBrush(Qt::blue));
}
void SpecificWorker::pintarposerobot()
{
	static QGraphicsEllipseItem *c;
	if(c!=NULL){
		scene->removeItem(c);
		c=NULL;
	}
	QVec r=map->operator[](robotnode);
	c = scene->addEllipse(r(0)/10-5, -r(2)/10-5, 10, 10, QPen(Qt::white), QBrush(Qt::red));
}
void SpecificWorker::borrarcirculo()
{
	if(circulo!=NULL){
		scene->removeItem(circulo);
		circulo=NULL;
	}
}
void SpecificWorker::pintarrobot()
{
	static QGraphicsEllipseItem *robot;
	differentialrobot_proxy->getBaseState(state);
	if(robot!=NULL){
		scene->removeItem(robot);
		robot=NULL;
	}
	QVec r=map->operator[](robotnode);
	robot = scene->addEllipse(state.x/10-5, -state.z/10-5, 15, 15, QPen(Qt::white), QBrush(Qt::cyan));

}
