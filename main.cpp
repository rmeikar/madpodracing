#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

struct Position {
    int x;
    int y;

    int magnitude() const {
        return hypot(x,y);
    };
};

struct Velocity {
    int x;
    int y;

    int magnitude() const {
        return hypot(x,y);
    };
};

//used for mirroring, mainly angles
int reverseDelta(const int& a, int b){
    return a + (a - b);
}

int angleBetween(const Position& a, Position& b) {
    return atan2(a.y - b.y, a.x - b.x) * 57.29578;
}

int distance(const Position& a, Position& b){
    return hypot(a.x - b.x, a.y - b.y);
}
float toRad(const int& a){
    return a / 57.29578;
}
//all pods are of class Pod
class Pod {
    public:
    Position position;
    Position velocity;
    Position target;
    //linear prediction of future positions
    Position predictPosition[5];
    bool boost = true;
    bool useBoost = false;
    bool useShield = false;
    //shield cooldown counter, currently unused
    int shieldCD = 0;
    //velocity angle
    int velAngle;
    //game given velocity angles in wrong angle system (0 to 360, instead of -180 to 180)
    int angle;
    //next checkpoint
    int nextCP = 1;
    //checkpoint count, inside the class for easier access
    int cpCount;
    int thrust;
    //checkpoint passed counter, for seeing who's leading
    int cpPassed = 0;
    //defence point for blocker pod, currently only for the finish checkpoint
    int blockpoint;
    Position blockerPosition;
    string message = "Start";
    //regular movement
    void move(const Position& a){
        target.x = a.x;
        target.y = a.y;
    }
    //movement command to point relative to the pod,
    void relativeMove(const Position& a) {
        target.x = position.x + a.x;
        target.y = position.y + a.y;
    };
    //movement command at a certain absolute angle
    void setAngle(const float& newAngle){
        target.x = position.x + 3000 * cos(toRad(newAngle));
        target.y = position.y + 3000 * sin(toRad(newAngle));
    };
    //check for angle difference between this pod and another
    int angleDifference(const Pod& a){
        return abs(velAngle - a.velAngle);
    }
    //movement command to point relative to another point
    Position offsetPosition(Position a, const int& x, const int& y){
        a.x = a.x + x;
        a.y = a.y + y;
        return a;
    }
    //general use movement command
    void compensatingMove(const Position& a){
        //if facing away from taret position, set angle towards to position
        if (abs(angleBetween(a, position) - velAngle) > 89 or (distance(a, position) > 4000))
        setAngle(angleBetween(a,position));

        //set angle towards the mirror angle of pod velocity and real absolute angle of pod to target position, bugs out when facing opposite way due to the nature of tan2, my trigonometry is too weak to fix
        else 
        setAngle(reverseDelta(angleBetween(a, position), velAngle));
    }
    //do a cool little slide for optimal movement
    void slideMove(const Position& a,Position b){
        int nextAngle = angleBetween(position,b);
        //thrust depends on the angle between current target and next one, the sharper the turn, the less thrust, could probably optimize, but works good enough currently
        thrust = fmax(0,fmin(100, 300*(distance(predictPosition[1], b) - distance(predictPosition[2], b))/velocity.magnitude()));
        //if (distance(position, b)>5000 and boost and abs(velAngle - angleBetween(b, position))<20) useBoost = true;
        //change angle to the next target position
        compensatingMove(b);
    }
    //puts slide and regular movement together, currently only racer pod uses this
    void dynamicMove(const Position& a,const Position& b){
        //check if about to pass checkpoint and velocity is enough for a slide
        if ((distance(a, predictPosition[3]) < 800 and velocity.magnitude() > 300) or (distance(a, predictPosition[2]) < 600 and velocity.magnitude() > 50)){
            slideMove(a,b); 
            message = "Sliding";
        }

        //else regular movement
        else {
            thrust = 100;
            compensatingMove(a); 
            message = "Compensating";
        }
    }

};

//find the leading enemy
Pod* leadingEnemy(vector<Pod*> enemyPods, Position checkpoint[]){
    //checks which of them has passed most checkpoints
    if (enemyPods[0]->cpPassed > enemyPods[1]->cpPassed) return enemyPods[0];
    else if (enemyPods[1]->cpPassed > enemyPods[0]->cpPassed) return enemyPods[1];
    //if both passed same amount of checkpoints, check which one is closest to the current one
    else if (distance(enemyPods[0]->position, checkpoint[enemyPods[0]->nextCP]) < 
            distance(enemyPods[1]->position, checkpoint[enemyPods[1]->nextCP])) 
            return enemyPods[0];
    else 
    return enemyPods[1];
}
//find the closest enemy to the pod
Pod* closestEnemy(Pod& pod, vector<Pod*> enemyPods){
    if (distance(pod.position, enemyPods[0]->position) < distance(pod.position, enemyPods[1]->position)) return enemyPods[0];
    else return enemyPods[1];
}

//AI logic for the blocker pod
void blockerAI(Pod& pod, Pod& allyPod, vector<Pod*> enemyPods, Position checkpoint[]){

    Pod& enemyPod = *leadingEnemy(enemyPods, checkpoint);
    //sets the checkpoint to block
    pod.blockerPosition = checkpoint[0];
    //finds the angle between the finish and the checkpoint before it
    int angle = angleBetween(checkpoint[0], checkpoint[pod.cpCount-1]);
    //sets the blocking position along the line between last two checkpoints, in an offset, so there is room for the racer pod to pass through without colliding
    pod.blockerPosition = pod.offsetPosition(pod.blockerPosition, 800 * cos(toRad(angle)), 800 * sin(toRad(angle)));
    //move towards the blocking position if close
    if (distance(pod.position, pod.blockerPosition) > 800){
        pod.message = "Going to position";
        pod.thrust = 35;
        pod.move(pod.blockerPosition);
    }
    //move towards the blocking position if far
    if (distance(pod.position, pod.blockerPosition) > 3000){
        pod.message = "Going to position long";
        pod.thrust = 80;
        pod.move(pod.blockerPosition);
    }
    //stop the pod because it has arrived
    if (distance(pod.position, pod.blockerPosition) < 800){
        pod.message = "I'm there";
        pod.move(enemyPod.predictPosition[1]);
        pod.thrust = 0;
    }
    //if the leading enemy pod is passing through, but not coming for the last checkpoint, ram it anyway to slow it down
    //checks if it's close enough and it's velocity angle is almost opposite the blocker pod angle, so that there would be a better chance of effective impact
    if (distance(enemyPod.position, pod.position) < 4000 and 
       (distance(checkpoint[0], pod.position) < 3000) and 
       (pod.angleDifference(enemyPod) < 225 and pod.angleDifference(enemyPod) > 135))
    {
        pod.move(enemyPod.predictPosition[3]);
        pod.message = "Free attack";
        pod.thrust = 100;
        if (distance(pod.predictPosition[1], enemyPod.predictPosition[1]) < 800) pod.useShield = true;
    }
    //if the leading enemy pod is coming towards blocked checkpoint, ram it
    if (enemyPod.nextCP == 0 and distance(enemyPod.position, checkpoint[0]) < 4000){
        pod.message = "Attack";
        pod.move(enemyPod.predictPosition[3]);
        pod.thrust = 100;
        cerr << "BOOST AVAILABLE? " << pod.boost << endl;
        //if boost is available, use it
        if (pod.boost and (pod.angleDifference(enemyPod) < 190 and pod.angleDifference(enemyPod) > 170)) pod.useBoost = true;
        //when about to collide, turn on shield
        if (distance(pod.predictPosition[1], enemyPod.predictPosition[1]) < 1000) {pod.useShield = true; pod.message = "Shield";}
    }

}

//AI logic for the racing pod
void racerAI(Pod& pod, Pod& allyPod, vector<Pod*> enemyPods, Position checkpoint[]){
    cerr << "racer velocity: " << pod.velocity.magnitude() << endl;
    Pod& enemyPod = *closestEnemy(pod, enemyPods);

    //movement
    pod.dynamicMove(checkpoint[pod.nextCP], checkpoint[(pod.nextCP+1)%pod.cpCount]);

    //shield logic, only use if opponent is about to directly collide with meaningful force
    if (distance(pod.predictPosition[1], enemyPod.predictPosition[1]) < 800 and 
       (enemyPod.velocity.magnitude() > 200 or 
       (enemyPod.velocity.magnitude() < 200 and pod.velocity.magnitude() > 200)) and
       (pod.angleDifference(enemyPod) > 90) and pod.angleDifference(enemyPod) < 270) 
        pod.useShield = true;
    
    cerr << "Enemy distance: " << distance(pod.position, enemyPod.position) << endl;
    cerr << "Enemy prediction distance: " << distance(pod.predictPosition[1], enemyPod.predictPosition[1]) << endl;
    cerr << "Enemy to point distance: " << distance(checkpoint[pod.nextCP], enemyPod.position) << endl;
    cerr << "Enemy velocity magnitude: " << enemyPod.velocity.magnitude() << endl;

    //if last checkpoint is the finish, go directly for the win
    if (pod.cpPassed == ((pod.cpCount*3)-1)) pod.compensatingMove(checkpoint[pod.nextCP]);

    //boost
    if (distance(pod.position, checkpoint[pod.nextCP]) > 6000 and
       pod.boost and 
       abs(pod.velAngle - angleBetween(checkpoint[pod.nextCP], pod.position)) < 30) 
       pod.useBoost = true;

    //hotfix for checkpoint orbiting
    if (distance(pod.position, checkpoint[pod.nextCP]) < 1000 and 
       pod.boost and abs(pod.velAngle - angleBetween(checkpoint[pod.nextCP], pod.position)) > 60) {
           cerr << "Fixing" << endl;
           pod.thrust = 70; 
           pod.move(checkpoint[pod.nextCP]);
           }

    //if about to pass through a checkpoint, but there are other pods in the way, start ramming through
    if ((distance(pod.predictPosition[1], enemyPod.predictPosition[1]) < 1000 and 
        (distance(checkpoint[pod.nextCP], enemyPod.position) < 3000))
        or 
        (distance(pod.predictPosition[1], allyPod.predictPosition[1]) < 1000 and 
        (distance(checkpoint[pod.nextCP], allyPod.position) < 3000))){
        pod.message = "Wrestling";
        pod.compensatingMove(checkpoint[pod.nextCP]);
    }
}

//Loading game-given data into the pod objects, process counters
void loadData(Pod& pod, int checkpoint_count){
    int lastCP = pod.nextCP;
    pod.cpCount = checkpoint_count;
    cin >> pod.position.x >> pod.position.y >> pod.velocity.x >> pod.velocity.y >> pod.angle >> pod.nextCP; cin.ignore();

    for (int i=0; i<5; i++){
        pod.predictPosition[i].x = pod.position.x + pod.velocity.x * i+1;
        pod.predictPosition[i].y = pod.position.y + pod.velocity.y * i+1;
    }

    pod.velAngle = angleBetween(pod.predictPosition[2],pod.position);
    pod.shieldCD = fmax(0, pod.shieldCD - 1);

    if (lastCP<pod.nextCP or (lastCP>pod.nextCP and pod.nextCP == 0)) ++pod.cpPassed;
}

//generate movement command outputs
void output(Pod& pod){
    cout << pod.target.x << " " << pod.target.y << " ";

    if (pod.useBoost) {cout << "BOOST"; pod.boost = false; pod.useBoost = false;}
    else if (pod.useShield) {cout << "SHIELD"; pod.shieldCD = 3; pod.useShield = false;}
    else {cout << pod.thrust;}

    cout << " " << pod.message << endl;
}
/**
 * Auto-generated code below aims at helping you parse
 * the standard input according to the problem statement.
 **/

int main()
{
    int laps;
    cin >> laps; cin.ignore();
    int checkpoint_count;
    cin >> checkpoint_count; cin.ignore();
    int checkpointX[checkpoint_count];
    int checkpointY[checkpoint_count];
    Position checkpoint[checkpoint_count];
    
    for (int i = 0; i < checkpoint_count; i++) {
        cin >> checkpoint[i].x >> checkpoint[i].y; cin.ignore();
    }

    Pod myPod1, myPod2; 
    Pod enemyPod1, enemyPod2;
    vector<Pod*> pods = {&myPod1, &myPod2, &enemyPod1, &enemyPod2};
    vector<Pod*> enemyPods = {&enemyPod1, &enemyPod2};

    myPod1.thrust = 100;
    myPod2.thrust = 100;
    // game loop
    while (1) {
        for (Pod* pod : pods){
            loadData(*pod, checkpoint_count);
        }
        // Write an action using cout. DON'T FORGET THE "<< endl"
        // To debug: cerr << "Debug messages..." << endl;
        cerr << myPod2.angle << endl;
        cerr << myPod2.velAngle << endl;
        cerr << angleBetween(checkpoint[myPod1.nextCP+1 % checkpoint_count],checkpoint[myPod1.nextCP]) << " " << myPod1.velAngle << endl;
        //if (abs(angleBetween(checkpoint[myPod1.nextCP], myPod1.position) - angleBetween(myPod1.position, myPod1.predictPosition[0])) < 90)
        racerAI(myPod1, myPod2, enemyPods, checkpoint);
        //else myPod1.target = checkpoint[myPod1.nextCP];
        blockerAI(myPod2, myPod1, enemyPods, checkpoint);
        // You have to output the target position
        // followed by the power (0 <= thrust <= 100)
        // i.e.: "x y thrust"
        output(myPod1); output(myPod2);
    }
}
