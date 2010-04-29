package edu.umich.soar.sproom.soar;

import april.lcmtypes.pose_t;
import sml.Agent;
import sml.Identifier;
import edu.umich.soar.IntWme;
import edu.umich.soar.StringWme;
import edu.umich.soar.sproom.Adaptable;
import edu.umich.soar.sproom.SharedNames;
import edu.umich.soar.sproom.command.Pose;
import edu.umich.soar.sproom.metamap.Area;
import edu.umich.soar.sproom.metamap.MapMetadata;

/**
 * Input link management of self state.
 *
 * @author voigtjr@gmail.com
 */
public class SelfIL implements InputLinkElement {
	private static final String NAME = "name";
	private static final String AREA = "area";

	private Identifier root;
	private IntWme areaId;
	private IntWme carryId;
	private DistanceWme[] xyz = new DistanceWme[3];
	private SpeedWme[] xyzVelocity = new SpeedWme[3];
	private YawWme yaw;
	private YawVelocityWme yawVelocity;
	
	public SelfIL(Identifier root, Adaptable app) {
		this.root = root;
		Agent agent = (Agent)app.getAdapter(Agent.class);
		
		StringWme.newInstance(root, NAME, agent.GetAgentName());
		areaId = IntWme.newInstance(root, AREA);
		
		Identifier pose = root.CreateIdWME(SharedNames.POSE);
		xyz[0] = DistanceWme.newInstance(pose, SharedNames.X);
		xyz[1] = DistanceWme.newInstance(pose, SharedNames.Y);
		xyz[2] = DistanceWme.newInstance(pose, SharedNames.Z);
		yaw = YawWme.newInstance(pose, SharedNames.YAW);
		xyzVelocity[0] = SpeedWme.newInstance(pose, SharedNames.X_VELOCITY);
		xyzVelocity[1] = SpeedWme.newInstance(pose, SharedNames.Y_VELOCITY);
		xyzVelocity[2] = SpeedWme.newInstance(pose, SharedNames.Z_VELOCITY);
		yawVelocity = YawVelocityWme.newInstance(pose, SharedNames.YAW_VELOCITY);
		
		update(app);
	}

	@Override
	public void update(Adaptable app) {
		Pose pose = (Pose)app.getAdapter(Pose.class);
		pose_t p = pose.getPose();
		
		Cargo cargo = (Cargo)app.getAdapter(Cargo.class);
		if (cargo.getCarriedObject() == null) {
			if (carryId != null) {
				carryId.destroy();
				carryId = null;
			}
		} else {
			if (carryId == null) {
				carryId = IntWme.newInstance(root, "carry");
			}
			carryId.update(cargo.getCarriedObject().getId());
		}
		
		MapMetadata metadata = (MapMetadata)app.getAdapter(MapMetadata.class);
		Area area = metadata.getArea(p.pos);
		areaId.update((area != null) ? area.getId() : -1);
		
		for (int i = 0; i < xyz.length; ++i) {
			xyz[i].update(p.pos[i]);
			xyzVelocity[i].update(p.vel[i]);
		}
		yaw.update(Pose.getYaw(p));
		yawVelocity.update(p.rotation_rate[2]);
	}

	@Override
	public void destroy() {
		this.root.DestroyWME();
	}
}
